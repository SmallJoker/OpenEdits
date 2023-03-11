#include "server.h"
#include "remoteplayer.h"
#include "core/blockmanager.h"
#include "core/chatcommand.h"
#include "core/packet.h"
#include "core/utils.h" // get_next_part
#include "core/world.h"
#include "server/database_auth.h"
#include "server/database_world.h"
#include "server/eeo_converter.h"
#include "version.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Server::Server() :
	Environment(new BlockManager()),
	m_chatcmd(this)
{
	puts("Server: startup");

	m_bmgr->doPackRegistration();

	m_con = new Connection(Connection::TYPE_SERVER, "Server");
	m_con->listenAsync(*this);

	{
		// Initialize persistent world storage
		m_world_db = new DatabaseWorld();
		if (!m_world_db->tryOpen("server_worlddata.sqlite")) {
			delete m_world_db;
			m_world_db = nullptr;
		}
	}

	{
		// Initialize auth
		m_auth_db = new DatabaseAuth();
		if (!m_auth_db->tryOpen("server_auth.sqlite")) {
			delete m_auth_db;
			m_auth_db = nullptr;
		}
	}

	{
		m_chatcmd.add("/help", (ChatCommandAction)&Server::chat_Help);
		m_chatcmd.add("/flags", (ChatCommandAction)&Server::chat_Flags);
		m_chatcmd.add("/ffilter", (ChatCommandAction)&Server::chat_FFilter);
		m_chatcmd.add("/fset", (ChatCommandAction)&Server::chat_FSet);
		m_chatcmd.add("/fdel", (ChatCommandAction)&Server::chat_FDel);
		// Owner
		m_chatcmd.add("/respawn", (ChatCommandAction)&Server::chat_Respawn);
		m_chatcmd.add("/clear", (ChatCommandAction)&Server::chat_Clear);
		m_chatcmd.add("/import", (ChatCommandAction)&Server::chat_Import);
		m_chatcmd.add("/save", (ChatCommandAction)&Server::chat_Save);
	}

	{
		PACKET_ACTIONS_MAX = 0;
		const ServerPacketHandler *handler = packet_actions;
		while (handler->func)
			handler++;

		PACKET_ACTIONS_MAX = handler - packet_actions;
		ASSERT_FORCED(PACKET_ACTIONS_MAX == (int)Packet2Server::MAX_END, "Packet handler mismatch");
	}
}

Server::~Server()
{
	puts("Server: stopping...");

	{
		// In case a packet is being processed
		SimpleLock lock(m_players_lock);

		for (auto it : m_players)
			delete it.second;
		m_players.clear();
	}

	delete m_con;
	delete m_auth_db;
	delete m_world_db;
	delete m_bmgr;
}


// -------------- Public members -------------

void Server::step(float dtime)
{
	// maybe run player physics?

	// always player lock first, world lock after.
	SimpleLock players_lock(m_players_lock);
	std::set<World *> worlds;
	for (auto p : m_players) {
		auto world = p.second->getWorld();
		if (!world)
			continue;

		worlds.emplace(world);

		auto &queue = world->proc_queue;
		if (queue.empty())
			continue;

		SimpleLock world_lock(world->mutex);

		Packet out;
		out.write(Packet2Client::PlaceBlock);

		for (auto it = queue.cbegin(); it != queue.cend();) {
			// Distribute valid changes to players

			out.write(it->peer_id); // continue
			// Write BlockUpdate
			it->write(out);

			DEBUGLOG("Server: sending block x=%d,y=%d,id=%d\n",
				it->pos.X, it->pos.Y, it->id);

			it = queue.erase(it);

			// Fit everything into an MTU
			if (out.size() > CONNECTION_MTU)
				break;
		}
		out.write<peer_t>(0); // end

		// Distribute to players within this world
		for (auto it : m_players) {
			if (it.second->getWorld() != world)
				continue;

			m_con->send(it.first, 0, out);
		}
	}

	for (World *world : worlds) {
		auto &meta = world->getMeta();

		for (auto &kdata : meta.keys) {
			if (kdata.step(dtime)) {
				// Disable keys

				kdata.active = false;
				bid_t block_id = (&kdata - meta.keys) + Block::ID_KEY_R;
				Packet out;
				out.write(Packet2Client::Key);
				out.write(block_id);
				out.write<u8>(false);

				for (auto it : m_players) {
					if (it.second->getWorld() != world)
						continue;

					m_con->send(it.first, 0, out);
				}
			}
		}
	}
	// No player physics (yet?)
}

// -------------- Utility functions --------------

RemotePlayer *Server::getPlayerNoLock(peer_t peer_id)
{
	auto it = m_players.find(peer_id);
	return it != m_players.end() ? dynamic_cast<RemotePlayer *>(it->second) : nullptr;
}

RefCnt<World> Server::getWorldNoLock(std::string &id)
{
	for (auto p : m_players) {
		auto world = p.second->getWorld();
		if (!world)
			continue;

		if (world->getMeta().id == id)
			return world;
	}
	return nullptr;
}

// -------------- Networking --------------

void Server::onPeerConnected(peer_t peer_id)
{
	if (0) {
		Packet pkt;
		pkt.write<Packet2Client>(Packet2Client::Quack);
		pkt.writeStr16("hello world");
		m_con->send(peer_id, 0, pkt);
	}
}

void Server::onPeerDisconnected(peer_t peer_id)
{
	SimpleLock lock(m_players_lock);

	auto player = getPlayerNoLock(peer_id);
	if (!player)
		return;

	printf("Server: Player %s disconnected\n", player->name.c_str());

	{
		Packet pkt;
		pkt.write(Packet2Client::Leave);
		pkt.write(peer_id);
		broadcastInWorld(player, 0, pkt);
	}

	player->setWorld(nullptr);
	m_players.erase(peer_id);

	delete player;
}

void Server::processPacket(peer_t peer_id, Packet &pkt)
{
	// one server instance, multiple worlds
	int action = (int)pkt.read<Packet2Server>();
	if (action >= PACKET_ACTIONS_MAX) {
		printf("Server: Packet action %u out of range\n", action);
		return;
	}

	const ServerPacketHandler &handler = packet_actions[action];

	SimpleLock lock(m_players_lock);

	if (handler.min_player_state != RemotePlayerState::Invalid) {
		RemotePlayer *player = getPlayerNoLock(peer_id);
		if (!player) {
			printf("Server: Player peer_id=%u not found.\n", peer_id);
			return;
		}
		if ((int)handler.min_player_state > (int)player->state) {
			printf("Server: peer_id=%u is not ready for action=%d.\n", peer_id, action);
			return;
		}
	}

	try {
		(this->*handler.func)(peer_id, pkt);
	} catch (std::out_of_range &e) {
		printf("Server: Action %d parsing error: %s\n", action, e.what());
	} catch (std::exception &e) {
		printf("Server: Action %d general error: %s\n", action, e.what());
	}
}

void Server::writeWorldData(Packet &out, World &world, bool is_clear)
{
	out.write(Packet2Client::WorldData);
	out.write<u8>(1 + is_clear); // 1: new data. 2: clear

	world.getMeta().writeCommon(out);
	blockpos_t size = world.getSize();
	out.write(size.X); // dimensions
	out.write(size.Y);
	if (!is_clear) {
		// TODO: make player-specific
		world.write(out, World::Method::Plain, PROTOCOL_VERSION);
	}
}

void Server::respawnPlayer(Player *player, bool send_packet)
{
	auto &meta = player->getWorld()->getMeta();
	auto blocks = player->getWorld()->getBlocks(Block::ID_SPAWN, nullptr);

	if (blocks.empty()) {
		player->pos = core::vector2df();
	} else {
		int index = meta.spawn_index;
		if (++index >= (int)blocks.size())
			index = 0;

		player->pos.X = blocks[index].X;
		player->pos.Y = blocks[index].Y;
		meta.spawn_index = index;
	}

	if (!send_packet)
		return;

	Packet pkt;
	pkt.write(Packet2Client::SetPosition);
	pkt.write<u8>(true); // reset progress
	pkt.write(player->peer_id);
	pkt.write(player->pos.X);
	pkt.write(player->pos.Y);
	pkt.write<peer_t>(0); // end of bulk

	m_con->send(player->peer_id, 1, pkt);
}


// -------------- Chat commands -------------

void Server::systemChatSend(Player *player, const std::string &msg)
{
	Packet pkt;
	pkt.write(Packet2Client::Chat);
	pkt.write<peer_t>(0);
	pkt.writeStr16(msg);
	m_con->send(player->peer_id, 0, pkt);
}


CHATCMD_FUNC(Server::chat_Help)
{
	std::string cmd(get_next_part(msg));
	if (cmd.empty()) {
		systemChatSend(player, "Use '/help CMD' for details. Available commands: " + m_chatcmd.dumpUI());
		return;
	}

	static const struct {
		const std::string cmd;
		const std::string text;
	} help_LUT[] = {
		{ "flags", "Syntax: /flags [PLAYERNAME]\nLists the provided (or own) player's flags." },
		{ "ffilter", "Syntax: //filter FLAG1 [...]\nLists all players matching the flag filter." },
		{ "fset", "Syntax: /fset PLAYERNAME FLAG1 [...]\nSets one or more flags for a player." },
		{ "fdel", "The complement of /fset" },
		// respawn
		{ "clear", "Syntax: /clear [W] [H]\nW,H: integer (optional) to specify the new world dimensions." },
		{ "import", "Syntax: /import FILENAME\nFILENAME: .eelvl format without the file extension" },
		// save
		{ "version", std::string("Server version: ") + VERSION_STRING },
	};

	// Trim "/" if necessary
	if (cmd[0] == '/')
		cmd = cmd.substr(1);

	for (auto v : help_LUT) {
		if (cmd != v.cmd)
			continue;

		std::string answer = v.text;

		auto main = m_chatcmd.get("/" + v.cmd);
		if (main) {
			std::string subcmds = main->dumpUI();
			if (!subcmds.empty()) {
				answer.append("\nSubcommands: ");
				answer.append(subcmds);
			}
		}
		systemChatSend(player, answer);
		return;
	}

	systemChatSend(player, "No help available for command " + cmd);
}

CHATCMD_FUNC(Server::chat_Flags)
{
	std::string who(get_next_part(msg));
	if (who.empty())
		who = player->name;
	else
		for (char &c : who)
			c = toupper(c);

	std::string ret;
	ret.append("Flags of player ");
	ret.append(who);
	ret.append(": ");
	// oh my fucking god
	ret.append(player->getWorld()->getMeta().getPlayerFlags(who).toHumanReadable());

	systemChatSend(player, ret);
}

CHATCMD_FUNC(Server::chat_FFilter)
{
	PlayerFlags specified(0);
	std::string flag_string(get_next_part(msg));
	while (!flag_string.empty()) {
		playerflags_t flags_new;
		if (!PlayerFlags::stringToPlayerFlags(flag_string, &flags_new)) {
			systemChatSend(player, "Unknown flag: " + flag_string +
				". Available flags: " + PlayerFlags::getFlagList());
			return;
		}
		specified.flags |= flags_new;

		flag_string = get_next_part(msg);
	}

	const auto &meta = player->getWorld()->getMeta();

	// List all players with flags
	std::string output("List of players with flags ");
	output.append(specified.toHumanReadable());
	output.append(":");

	for (auto it : meta.getAllPlayerFlags()) {
		if ((it.second.flags | specified.flags) == 0)
			continue;
		output.append(" " + it.first);
	}

	systemChatSend(player, output);
}

bool Server::changePlayerFlags(Player *player, std::string msg, bool do_add)
{
	const PlayerFlags myflags = player->getFlags();
	if (!myflags.check(PlayerFlags::PF_HELPER)) {
		systemChatSend(player, "Insufficient permissions");
		return false;
	}

	auto world = player->getWorld();
	auto &meta = player->getWorld()->getMeta();

	std::string playername(get_next_part(msg));
	for (char &c : playername)
		c = toupper(c);

	// Search for existing records
	peer_t target_peer_id = 0;
	PlayerFlags targetflags = meta.getPlayerFlags(playername);

	// Search for online players
	for (auto p : m_players) {
		if (p.second->getWorld() != world)
			continue;
		if (p.second->name != playername)
			continue;

		target_peer_id = p.first;
		break;
	}

	if (target_peer_id == 0 && targetflags.flags == 0) {
		systemChatSend(player, "Cannot find player " + playername);
		return false;
	}

	// Flags that current player is allowed to change
	playerflags_t allowed_to_change = 0;
	if (player->name == meta.owner)
		allowed_to_change = PlayerFlags::PF_CNG_MASK_OWNER;
	else if (myflags.check(PlayerFlags::PF_OWNER))
		allowed_to_change = PlayerFlags::PF_CNG_MASK_COOWNER;
	else if (myflags.check(PlayerFlags::PF_HELPER))
		allowed_to_change = PlayerFlags::PF_CNG_MASK_HELPER;

	// Read in all specified flags
	playerflags_t flags_specified = 0;
	std::string flag_string(get_next_part(msg));
	while (!flag_string.empty()) {
		playerflags_t flags_new;
		if (!PlayerFlags::stringToPlayerFlags(flag_string, &flags_new)) {
			systemChatSend(player, "Unknown flag: " + flag_string +
				". Available flags: " + PlayerFlags::getFlagList());
			return false;
		}
		flags_specified |= flags_new;

		flag_string = get_next_part(msg);
	}

	if ((flags_specified | allowed_to_change) != allowed_to_change) {
		systemChatSend(player, "Insufficient permissions");
		return false;
	}

	// Perform the operation
	if (do_add) {
		targetflags.set(flags_specified, flags_specified);
	} else {
		targetflags.set(0, flags_specified);
	}

	meta.setPlayerFlags(playername, targetflags);
	// Get up-to-date information
	targetflags = meta.getPlayerFlags(playername);

	chat_Flags(player, playername);

	// Notify the player about this change
	if (target_peer_id != 0 && do_add
			&& (targetflags.flags & (PlayerFlags::PF_BANNED | PlayerFlags::PF_TMP_HEAVYKICK))) {

		sendMsg(target_peer_id, player->name + " kicked/banned you from this world.");
		{
			Packet out;
			out.write<Packet2Client>(Packet2Client::Leave);
			out.write(target_peer_id);
			m_con->send(target_peer_id, 0, out);
		}

		target_peer_id = 0;
	}

	if (target_peer_id != 0) {
		Packet out;
		out.write(Packet2Client::PlayerFlags);
		out.write<playerflags_t>(targetflags.flags & flags_specified); // new flags
		out.write<playerflags_t>(flags_specified); // mask
		m_con->send(target_peer_id, 1, out);
	}

	return true;
}

CHATCMD_FUNC(Server::chat_FSet)
{
	changePlayerFlags(player, msg, true);
}

CHATCMD_FUNC(Server::chat_FDel)
{
	changePlayerFlags(player, msg, false);
}

CHATCMD_FUNC(Server::chat_Respawn)
{
	respawnPlayer(player, true);
}

/// cmd: /clear [width] [height]
CHATCMD_FUNC(Server::chat_Clear)
{
	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Insufficient permissions");
		return;
	}

	std::string width_s(get_next_part(msg));
	std::string height_s(get_next_part(msg));

	auto old_world = player->getWorld();
	int64_t width_i = -1,
		height_i = -1;

	if (width_s.empty()) {
		width_i = old_world->getSize().X;
		height_i = old_world->getSize().Y;
	} else if (string2int64(width_s.c_str(), &width_i)) {
		if (height_s.empty())
			height_i = width_i;
		else
			string2int64(height_s.c_str(), &height_i);
	}

	if ((width_i < 3 || width_i > 300)
			|| (height_i < 3 || height_i > 300)) {
		systemChatSend(player, "Width and height must be in the range [3,300]");
		return;
	}

	RefCnt<World> world(new World(old_world));
	world->drop(); // kept alive by RefCnt

	try {
		world->createEmpty(blockpos_t(width_i, height_i));
	} catch (std::runtime_error &e) {
		systemChatSend(player, std::string("ERROR: ") + e.what());
		return;
	}

	Packet out;
	writeWorldData(out, *world.ptr(), true);

	for (auto it : m_players) {
		if (it.second->getWorld() == old_world) {
			it.second->setWorld(world.ptr());
			m_con->send(it.first, 0, out);
		}
	}

	systemChatSend(player, "Cleared!");
}

CHATCMD_FUNC(Server::chat_Import)
{
	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Missing permissions");
		return;
	}

	std::string filename(strtrim(msg));
	filename += ".eelvl";

	if (filename.find('/') != std::string::npos
			|| filename.find('\\') != std::string::npos) {
		systemChatSend(player, "The file must be located next to the server executable.");
		return;
	}

	auto old_world = player->getWorld();

	RefCnt<World> world(new World(old_world));
	world->drop(); // kept alive by RefCnt

	EEOconverter conv(*world.ptr());

	try {
		conv.fromFile(filename);
	} catch (std::runtime_error &e) {
		systemChatSend(player, std::string("ERROR: ") + e.what());
		return;
	}

	systemChatSend(player, "Imported!");

	Packet out;
	writeWorldData(out, *world.ptr(), false);

	for (auto it : m_players) {
		if (it.second->getWorld() == old_world) {
			it.second->setWorld(world.ptr());
			m_con->send(it.first, 0, out);
			respawnPlayer(it.second, true);
		}
	}
}

CHATCMD_FUNC(Server::chat_Save)
{
	if (!m_world_db)
		return;

	auto world = player->getWorld();

	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Missing permissions");
		return;
	}

	m_world_db->save(world);

	systemChatSend(player, "Saved!");
}
