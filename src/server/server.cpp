#include "server.h"
#include "remoteplayer.h"
#include "core/blockmanager.h"
#include "core/chatcommand.h"
#include "core/packet.h"
#include "core/utils.h" // get_next_part
#include "core/world.h"
#include "server/database_world.h"
#include "server/eeo_converter.h"

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
		m_chatcmd.add("/help", (ChatCommandAction)&Server::chat_Help);
		m_chatcmd.add("/flags", (ChatCommandAction)&Server::chat_Flags);
		// Owner
		m_chatcmd.add("/respawn", (ChatCommandAction)&Server::chat_Respawn);
		m_chatcmd.add("/clear", (ChatCommandAction)&Server::chat_Clear);
		m_chatcmd.add("/import", (ChatCommandAction)&Server::chat_Import);
		m_chatcmd.add("/save", (ChatCommandAction)&Server::chat_Save);;
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

	if (0) {
		Packet pkt;
		pkt.write<Packet2Client>(Packet2Client::Error);
		pkt.writeStr16("No error. Everything's fine");
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
			printf("Server: Player peer_id=%d not found.\n", peer_id);
			return;
		}
		if ((int)handler.min_player_state > (int)player->state) {
			printf("Server: peer_id=%d is not ready for action=%d.\n", peer_id, action);
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
		world.write(out, World::Method::Plain, 9999);
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
	systemChatSend(player, "Available commands: " + m_chatcmd.dumpUI());
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

CHATCMD_FUNC(Server::chat_Respawn)
{
	respawnPlayer(player, true);
}

CHATCMD_FUNC(Server::chat_Clear)
{
	if (!player->getFlags().check(PlayerFlags::PF_OWNER)) {
		systemChatSend(player, "Missing permissions");
		return;
	}

	auto world = player->getWorld();
	for (Block *b = world->begin(); b != world->end(); ++b)
		*b = Block();

	Packet out;
	writeWorldData(out, *world, true);
	broadcastInWorld(player, 0, out);

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

	RefCnt<World> world(new World(m_bmgr, old_world->getMeta().id));
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
