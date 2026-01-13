#include "server.h"
#include "database_auth.h"
#include "database_world.h"
#include "remoteplayer.h"
#include "servermedia.h"
#include "serverscript.h"
#include "core/blockmanager.h"
#include "core/eeo_converter.h"
#include "core/friends.h"
#include "core/logger.h"
#include "core/network_enums.h"
#include "core/packet.h"
#include "core/utils.h"
#include "core/world.h"
#include "core/worldmeta.h"
#include "core/script/scriptevent.h"
#include <set>

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif
static Logger logger("ServerPkt", LL_DEBUG);

// in sync with core/packet.h
const ServerPacketHandler Server::packet_actions[] = {
	{ RemotePlayerState::Invalid,   &Server::pkt_Quack }, // 0
	{ RemotePlayerState::Invalid,   &Server::pkt_Hello },
	{ RemotePlayerState::Login,     &Server::pkt_Auth },
	{ RemotePlayerState::Idle,      &Server::pkt_GetLobby },
	{ RemotePlayerState::Idle,      &Server::pkt_Join },
	{ RemotePlayerState::WorldJoin, &Server::pkt_Leave }, // 5
	{ RemotePlayerState::WorldPlay, &Server::pkt_Move },
	{ RemotePlayerState::WorldPlay, &Server::pkt_Chat },
	{ RemotePlayerState::WorldPlay, &Server::pkt_PlaceBlock },
	{ RemotePlayerState::WorldPlay, &Server::pkt_TriggerBlocks },
	{ RemotePlayerState::WorldPlay, &Server::pkt_GodMode }, // 10
	{ RemotePlayerState::WorldPlay, &Server::pkt_Smiley },
	{ RemotePlayerState::Idle,      &Server::pkt_FriendAction },
	{ RemotePlayerState::Idle,      &Server::pkt_MediaRequest },
	{ RemotePlayerState::WorldPlay, &Server::pkt_ScriptEvent },
	{ RemotePlayerState::Invalid, 0 }
};

void Server::pkt_Quack(peer_t peer_id, Packet &pkt)
{
	logger(LL_PRINT, "Quack! %zu bytes from peer_id=%u\n", pkt.size(), peer_id);
}

void Server::pkt_Hello(peer_t peer_id, Packet &pkt)
{
	uint16_t protocol_max = pkt.read<uint16_t>();
	uint16_t protocol_min = pkt.read<uint16_t>();

	const uint16_t protocol_ver = std::min(PROTOCOL_VERSION_MAX, protocol_max);
	if (protocol_ver < protocol_min || protocol_ver < PROTOCOL_VERSION_MIN) {
		char buf[255];
		snprintf(buf, sizeof(buf), "server=[%d,%d], client=[%d,%d]",
			protocol_min, protocol_max, PROTOCOL_VERSION_MIN, PROTOCOL_VERSION_MAX);

		logger(LL_PRINT, "Protocol mismatch. peer_id=%u tried to connect: %s\n", peer_id, buf);
		sendMsg(peer_id, std::string("Incompatible protocol versions. ") + buf);
		m_con->disconnect(peer_id);
		return;
	}

	std::string name(pkt.readStr16());
	to_player_name(name);

	bool ok = name.size() <= 16 && name.size() >= 3 && isalnum_nolocale(name);

	if (!ok) {
		sendMsg(peer_id, "Invalid nickname (must be [A-z0-9]{3,16})");
		m_con->disconnect(peer_id);
		return;
	}

	auto player = new RemotePlayer(peer_id, protocol_ver);
	auto insertion = m_players.emplace(peer_id, player);
	if (!insertion.second) {
		delete player;
		return;
	}

	player->name = name;
	player->state = RemotePlayerState::Login;

	u8 init_flags = pkt.read<u8>();
	player->media.needs_audiovisuals = init_flags & 1;

	{
		// Confirm
		Packet reply = player->createPacket(Packet2Client::Hello);
		reply.write(player->protocol_version);
		reply.write(player->peer_id);
		reply.writeStr16(player->name);

		m_con->send(peer_id, 0, reply);
	}

	logger(LL_PRINT, "Hello from %s, proto_ver=%d\n", player->name.c_str(), player->protocol_version);

	// Auth
	bool is_guest = player->name.rfind("GUEST", 0) == 0;
	if (is_guest) {
		// Ensure digits only
		for (char c : player->name.substr(5)) {
			if (!std::isdigit(c)) {
				is_guest = false;
				break;
			}
		}

		if (!is_guest) {
			sendMsg(peer_id, "Temporary accounts (guest) must follow the naming scheme GUEST[0-9]*");
			m_con->disconnect(peer_id);
			return;
		}
	}

	if (is_guest) {
		player->auth.status = Auth::Status::Guest;

		// Log in instantly
		signInPlayer(player);
		return;
	}

	if (!m_auth_db) {
		sendMsg(peer_id, "Server error: Auth database error. Please use GuestXXXX accounts.");
		m_con->disconnect(peer_id);
		return;
	}

	// Prompt for authentication if needed
	AuthAccount info;
	ok = m_auth_db->load(player->name, &info);
	if (ok) {
		player->auth.status = Auth::Status::Unauthenticated;
		player->auth.salt_challenge = Auth::generateRandom();

		Packet out;
		out.write(Packet2Client::Auth);
		out.writeStr16("login1");
		out.writeStr16(m_auth_db->getUniqueSalt());
		out.writeStr16(player->auth.salt_challenge); // challenge
		m_con->send(peer_id, 0, out);
	} else {
		player->auth.status = Auth::Status::Unregistered;

		Packet out;
		out.write(Packet2Client::Auth);
		out.writeStr16("register");
		out.writeStr16(m_auth_db->getUniqueSalt());
		m_con->send(peer_id, 0, out);
	}
}

void Server::signInPlayer(RemotePlayer *player)
{
	player->setScript(m_script);
	player->state = RemotePlayerState::Idle;
	logger(LL_PRINT, "Player %s logged in\n", player->name.c_str());

	{
		Packet out;
		out.write(Packet2Client::Auth);
		out.writeStr16("signed_in");
		m_con->send(player->peer_id, 0, out);
	}

	ASSERT_FORCED(m_media, "Missing ServerMedia");

	{
		Packet out = player->createPacket(Packet2Client::MediaList);
		m_media->writeMediaList(player, out);
		m_con->send(player->peer_id, 0, out);
	}
}


void Server::pkt_Auth(peer_t peer_id, Packet &pkt)
{
	if (!m_auth_db) {
		sendMsg(peer_id, "Service unavailable");
		return;
	}

	RemotePlayer *player = getPlayerNoLock(peer_id);
	std::string action = pkt.readStr16();

	if (player->auth.status == Auth::Status::Guest) {
		sendMsg(peer_id, "Cannot perform auth actions for guests.");
		return;
	}

	// Authenticate by password (hash challenge)
	if (action == "login2") {
		if (player->auth.status != Auth::Status::Unauthenticated)
			return;

		std::string address = m_con->getPeerAddress(peer_id);
		{
			// Prevent password brute-force attacks
			if (m_auth_db->getBanRecord(address, action, nullptr)) {
				sendMsg(peer_id, "Too many login requests. Please wait a few seconds.");
				return;
			}
		}

		// Confirm the client-sent hash

		AuthAccount info;
		bool ok = m_auth_db->load(player->name, &info);
		if (!ok) {
			sendMsg(peer_id, "Invalid action");
			return;
		}

		// Compare doubly-hashed passwords
		player->auth.hash(info.password, player->auth.salt_challenge);

		const std::string hash = pkt.readStr16();
		bool signed_in = player->auth.output == hash;
		if (info.password.empty()) {
			sendMsg(peer_id, "No password saved. Change your password with /setpass !");
			signed_in = true;
		}

		if (!signed_in) {
			{
				AuthBanEntry entry;
				entry.affected = address;
				entry.context = action;
				entry.expiry = time(nullptr) + 2;
				m_auth_db->ban(entry);
			}
			sendMsg(peer_id, "Incorrect password");
			m_con->disconnect(peer_id);
			return;
		}

		player->auth.salt_challenge.clear(); // would allow to change the password directly
		player->auth.status = Auth::Status::SignedIn;

		signInPlayer(player);

		// Update last login timestamp
		info.last_login = time(nullptr);
		m_auth_db->save(info);
		return;
	}

	// Register new account, password provided.
	if (action == "register") {
		if (player->auth.status != Auth::Status::Unregistered)
			return;

		AuthAccount info;
		bool ok = m_auth_db->load(player->name, &info);
		if (ok) {
			sendMsg(peer_id, "This user is already registered.");
			return;
		}

		std::string address = m_con->getPeerAddress(peer_id);
		{
			// Prevent register spam
			if (m_auth_db->getBanRecord(address, action, nullptr)) {
				sendMsg(peer_id, "Too many account creation requests.");
				return;
			}
		}

		info.name = player->name;
		info.password = pkt.readStr16();
		info.level = AuthAccount::AL_REGISTERED;

		if (info.password.empty()) {
			sendMsg(peer_id, "Empty password. Something went wrong.");
			return;
		}

		m_auth_db->save(info);
		sendMsg(peer_id, "Account registered. You may now login.");

		{
			AuthLogEntry log;
			log.subject = player->name;
			log.text = "registered";
			m_auth_db->logNow(log);
		}

		{
			AuthBanEntry entry;
			entry.affected = address;
			entry.context = action;
			entry.expiry = time(nullptr) + 60;
			m_auth_db->ban(entry);
		}
		return;
	}

	// Update password with specified hash
	if (action == "setpass") {
		if (player->auth.status != Auth::Status::SignedIn)
			return;

		AuthAccount info;
		bool ok = m_auth_db->load(player->name, &info);
		if (!ok) {
			sendMsg(peer_id, "Cannot change password: Auth not found.");
			return;
		}

		std::string old_pw = pkt.readStr16();
		if (!info.password.empty() && info.password != old_pw) {
			sendMsg(peer_id, "Incorrect password");
			return;
		}

		// Update to newly specified password (insecure)
		info.password = pkt.readStr16();

		ok = m_auth_db->save(info);
		if (!ok) {
			sendMsg(peer_id, "Failed to change password.");
			return;
		}

		sendMsg(peer_id, "Password changed successfully.");

		Packet out;
		out.write(Packet2Client::Auth);
		out.writeStr16("pass_set");
		m_con->send(peer_id, 0, out);
		return;
	}

	sendMsg(peer_id, "Unknown auth action: " + action);
}

void Server::pkt_MediaRequest(peer_t peer_id, Packet &pkt)
{
	ASSERT_FORCED(m_media, "Missing ServerMedia");

	RemotePlayer *player = getPlayerNoLock(peer_id);
	m_media->readMediaRequest(player, pkt);

	// data is sent in step()
}

void Server::pkt_GetLobby(peer_t peer_id, Packet &)
{
	const RemotePlayer *player = getPlayerNoLock(peer_id);

	Packet out = player->createPacket(Packet2Client::Lobby);

	std::set<std::string> listed_world_ids;

	std::set<RefCnt<World>> worlds;
	for (auto &p : m_players) {
		auto world = p.second->getWorld();
		if (world)
			worlds.insert(world);
	}

	// Currently online worlds
	for (auto world : worlds) {
		const auto &meta = world->getMeta();
		listed_world_ids.insert(meta.id);
		if (!meta.is_public)
			continue;

		out.write<u8>(true); // continue!

		meta.writeCommon(out);
		// Additional Lobby fields
		blockpos_t size = world->getSize();
		out.write(size.X);
		out.write(size.Y);
	}

	if (!m_static_lobby_worlds_timer.isActive()) {
		m_static_lobby_worlds_timer.set(60);
		EEOconverter::listImportableWorlds(m_importable_worlds);
		if (m_world_db)
			m_featured_worlds = m_world_db->getFeatured();
	}

	auto add_worlds_from_vector = [&listed_world_ids, &out] (const std::vector<LobbyWorld> &worlds) {
		for (const auto &meta : worlds) {
			auto it = listed_world_ids.find(meta.id);
			if (it != listed_world_ids.end())
				continue; // already sent

			listed_world_ids.insert(meta.id);
			out.write<u8>(true); // continue!

			meta.writeCommon(out);
			// Additional Lobby fields
			out.write(meta.size.X);
			out.write(meta.size.Y);
		}
	};

	if (m_world_db) {
		// This player's worlds
		auto found = m_world_db->getByPlayer(player->name);
		add_worlds_from_vector(found);

		// Featured worlds
		add_worlds_from_vector(m_featured_worlds);
	}

	// EELVL importable worlds
	{

		for (const auto &[filename, meta] : m_importable_worlds) {
			out.write<u8>(true); // continue!
			meta.writeCommon(out);
			// Additional Lobby fields
			out.write(meta.size.X);
			out.write(meta.size.Y);
		}
	}

	out.write<u8>(false); // done with worlds

	// Friends
	if (m_auth_db) {
		std::vector<AuthFriend> friends;
		m_auth_db->listFriends(player->name, &friends);
		for (const AuthFriend &f : friends) {
			Player *other = findPlayer(nullptr, f.p2.name, true);
			RefCnt<World> world;
			int status = f.p2.status;

			if (f.p1.status == (int)LobbyFriend::Type::Pending)
				status = (int)LobbyFriend::Type::PendingIncoming;
			// NO ELSE-IF! Do not leak the world ID for incoming requests
			if (status == (int)LobbyFriend::Type::Accepted) {
				if (other)
					world = other->getWorld();

				status = other
					? (int)LobbyFriend::Type::FriendOnline
					: (int)LobbyFriend::Type::FriendOffline;
			}

			out.write<u8>(true); // (1) next item
			out.write<u8>(status);
			out.writeStr16(f.p2.name);

			if (world)
				out.writeStr16(world->getMeta().id);
			else
				out.writeStr16("");
		}

		// Examples for client test
		for (u8 type = 0; type < (u8)LobbyFriend::Type::MAX_INVALID; ++type) {
			out.write<u8>(true);
			out.write(type);
			out.writeStr16("FOOBAR" + std::to_string(type));
			out.writeStr16("WORLDID");
		}

		out.write<u8>(false); // done with friends
	}

	m_con->send(peer_id, 0, out);
}

void Server::pkt_Join(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	std::string world_id(pkt.readStr16());
	world_id = strtrim(world_id);
	bool create_world = world_id.empty();

	WorldMeta::Type world_type = WorldMeta::Type::Persistent;
	blockpos_t size { 100, 75 };
	std::string title, code;

	if (pkt.getRemainingBytes() > 0) {
		world_type = (WorldMeta::Type)pkt.read<u8>();
		pkt.read(size.X);
		pkt.read(size.Y);
		title = pkt.readStr16();
		code  = strtrim(pkt.readStr16());
	}

	if (create_world) {
		// See also: WorldMeta::idToType
		switch (world_type) {
			case WorldMeta::Type::TmpSimple:
			case WorldMeta::Type::TmpDraw:
				world_id = "T" + generate_world_id(6);
				break;
			case WorldMeta::Type::Persistent:
				world_id = "P" + generate_world_id(6);
				break;
			default:
				sendMsg(peer_id, "Unsupported world creation type.");
				return;
		}

		// World size check
		std::string err_msg;
		if (!checkSize(err_msg, size)) {
			sendMsg(peer_id, err_msg);
			return;
		}

		// Title check
		err_msg.clear();
		if (!checkTitle(err_msg, title)) {
			sendMsg(peer_id, err_msg);
			return;
		}
		if (!err_msg.empty())
			sendMsg(peer_id, err_msg);

		// World ID check
		bool is_ok = world_id.size() >= 4 && world_id.size() <= 15 && isalnum_nolocale(world_id);
		if (!is_ok) {
			sendMsg(peer_id, "Invalid world ID. [A-z0-9]{4,15} are allowed");
			return;
		}
	}

	// query database for existing world
	auto world = getWorldNoLock(world_id);
	if (!world) {
		world = std::make_shared<World>(m_bmgr, world_id);

		if (!loadWorldNoLock(world.get()))
			world.reset(); // Not found
	}

	if (!world && WorldMeta::idToType(world_id) == WorldMeta::Type::Readonly) {
		std::string path = EEOconverter::findWorldPath(world_id);
		if (!path.empty()) {
			world = std::make_shared<World>(m_bmgr, world_id);
			try {
				EEOconverter conv(*world.get());
				conv.fromFile(path);
			} catch (std::runtime_error &e) {
				sendMsg(peer_id, std::string("Cannot load this world: ") + e.what());
				return;
			}
			world->getMeta().owner += " "; // HACK: prevent modifications
		}
	}

	if (!world && !create_world) {
		sendMsg(peer_id, "The specified world ID does not exist.");
		return;
	}

	if (!world && create_world && world_type == WorldMeta::Type::Persistent) {
		// Guests cannot create worlds
		if (player->auth.status == Auth::Status::Guest) {
			sendMsg(peer_id, "Guests cannot create persistent worlds.");
			return;
		}
	}

	if (m_auth_db) {
		AuthBanEntry entry;
		if (m_auth_db->getBanRecord(player->name, world_id, &entry)) {
			int64_t minutes_i = (entry.expiry - time(nullptr) + 59) / 60;
			sendMsg(peer_id, "You are banned from this world for " +
				std::to_string(minutes_i) + " minute(s). Reason: " + entry.comment);
			return;
		}
	}

	if (!world) {
		// create a new one
		world = std::make_shared<World>(m_bmgr, world_id);

		world->createDummy(size);
		world->getMeta().title = title;
		world->getMeta().type = world_type;
		if (world_type == WorldMeta::Type::Persistent)
			world->getMeta().owner = player->name;
		else
			world->getMeta().edit_code = code;
	}

	{
		// Allow only one of each account per world
		for (auto &p : m_players) {
			if (p.second->name != player->name)
				continue;

			if (p.second->getWorld() == world) {
				sendMsg(peer_id, "You already joined this world.");
				return;
			}
		}
	}

	{
		Packet out;
		out.data_version = player->protocol_version;
		writeWorldData(out, *world.get(), false);
		m_con->send(peer_id, 0, out);
	}

	{
		// Update player information
		player->setWorld(world);
		respawnPlayer(player, false);
		player->state = RemotePlayerState::WorldPlay;
	}

	if (m_script)
		m_script->onPlayerEvent("join", player);

	// Notify about new player
	auto make_join_packet = [](RemotePlayer *player, Packet &out) {
		out.write(Packet2Client::Join);
		out.write(player->peer_id);
		out.writeStr16(player->name);
		out.write<u8>(player->godmode);
		out.write<u8>(player->smiley_id);
		player->writePhysics(out);
	};

	// Announce this player to all those who already joined
	broadcastInWorld(player, RemotePlayerState::WorldPlay, 0, SERVER_PKT_CB {
		make_join_packet(player, out);
	});

	// Announce all existing players to the current player
	for (auto &it : m_players) {
		RemotePlayer *p2 = (RemotePlayer *)it.second.get();
		if (p2 == player)
			continue; // already sent
		if (p2->getWorld() != world)
			continue;

		// Notify new player about existing ones

		Packet out;
		out.data_version = p2->protocol_version;
		make_join_packet(p2, out);
		m_con->send(peer_id, 0, out);
	}

	// Chat history replay
	{
		Packet out;
		out.write(Packet2Client::ChatReplay);
		auto &meta = world->getMeta();
		for (const WorldMeta::ChatHistory &entry : meta.chat_history) {
			out.write<s64>(entry.timestamp);
			out.writeStr16(entry.name);
			out.writeStr16(entry.message);
		}
		if (out.size() > 2)
			m_con->send(peer_id, 0, out);
	}

	// Player flags
	{
		setDefaultPlayerFlags(player);

		// Send all player flags where needed
		Packet pkt_new;
		pkt_new.write(Packet2Client::PlayerFlags);
		player->writeFlags(pkt_new, PlayerFlags::PF_MASK_SEND_OTHERS);

		Packet out;
		out.write(Packet2Client::PlayerFlags);
		// Send all flags to the player
		for (auto &p : m_players) {
			if (p.second->getWorld() != world)
				continue;

			// Notify existing players
			m_con->send(p.first, 0, pkt_new);

			// Append for new player
			p.second->writeFlags(out, PlayerFlags::PF_MASK_SEND_PLAYER);
		}

		if (out.size() > 2)
			m_con->send(player->peer_id, 0, out);
	}

	printf("Server: Player %s joined world id=%s\n",
		player->name.c_str(), world->getMeta().id.c_str()
	);
}

void Server::pkt_Leave(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	printf("Server: Player %s left world id=%s\n",
		player->name.c_str(), player->getWorld()->getMeta().id.c_str()
	);

	sendPlayerLeave(player);
}

void Server::pkt_Move(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	ASSERT_FORCED(player, "Player required!");

	/* TODO reject old (unreliable) packets
		u8 seqnum = pkt.read<u8>();
		if (wrapped_dist_u8(seqnum, player->seqnum) < 0)
			return; // outdated
		player->seqnum = seqnum;
	*/

	player->readPhysics(pkt);
	player->dtime_delay = m_con->getPeerRTT(peer_id) / 2.0f;
	player->runAnticheat(player->time_since_move_pkt);
	player->time_since_move_pkt = 0;

	// broadcast to connected players
	broadcastInWorld(player, RemotePlayerState::WorldPlay, 1 | Connection::FLAG_UNRELIABLE,
			SERVER_PKT_CB {
		out.write(Packet2Client::Move);

		// Bulk sending (future)
		out.write(peer_id);
		player->writePhysics(out);
	});
}

void Server::pkt_Chat(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	std::string message(pkt.readStr16());
	if (message.size() > 200)
		message.resize(200);

	message = strtrim(message);

	bool ok = message.size() > 0;
	for (uint8_t c : message) {
		if (c < 0x20 || c == 0x7F) {
			ok = false;
			break;
		}
	}

	if (!ok) {
		sendMsg(peer_id, "Control characters are not allowed");
		return;
	}

	bool was_active = player->rl_chat.isActive();
	if (player->rl_chat.add(1)) {
		if (!was_active)
			sendMsg(peer_id, "Chat message ignored. Please chat less.");
		return;
	}

	if (m_chatcmd.run(player, message))
		return; // Handled

	if (message[0] == '/') {
		// Unknown command
		systemChatSend(player, "Unknown command. See /help");
		return;
	}

	if (player->getFlags().check(PlayerFlags::PF_MUTED))
		return;

	{
		auto &meta = player->getWorld()->getMeta();
		meta.trimChatHistory(15);

		WorldMeta::ChatHistory h;
		h.timestamp = time(nullptr);
		h.name = player->name;
		h.message = message;
		meta.chat_history.push_back(std::move(h));
	}

	Packet out;
	out.write(Packet2Client::Chat);
	out.write(peer_id);
	out.writeStr16(message);

	broadcastInWorld(player, 1, out);
}

void Server::pkt_PlaceBlock(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	PlayerFlags pflags = player->getFlags();
	if ((pflags.flags & PlayerFlags::PF_EDIT_DRAW) == 0)
		return; // Missing permissions

	// Cooldown for block placement, especially for players without draw
	float value = pflags.check(PlayerFlags::PF_EDIT_DRAW) ?
		1.0f : 5.0f;

	auto world = player->getWorld();
	SimpleLock lock(world->mutex);

	if (m_script)
		m_script->setPlayer(player);

	BlockUpdate bu(world->getBlockMgr());
	while (pkt.getRemainingBytes()) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		if (player->rl_blocks.add(value))
			return; // WARNING: Remaining data in pkt!

		bu.peer_id = peer_id;
		bu.read(pkt);

		if (!world->checkUpdateBlockNeeded(bu))
			continue;

		if (m_script) {
			// Block placement rejected by script
			if (!m_script->onBlockPlace(bu))
				continue;
		}

		(void)world->updateBlockNoCheck(bu);
		// Put into queue to keep the world lock as short as possible
		world->proc_queue.insert(bu);
	}
}

void Server::pkt_TriggerBlocks(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	auto world = player->getWorld();
	auto &meta = world->getMeta();

	// TODO: Check whether the responsible player is (or was) nearby
	bool is_dead = false;
	while (pkt.getRemainingBytes()) {
		blockpos_t pos;
		pkt.read(pos.X);
		if (pos.X == BLOCKPOS_INVALID)
			break;
		pkt.read(pos.Y);

		Block b;
		world->getBlock(pos, &b);
		switch (b.id) {
			case Block::ID_KEY_R:
			case Block::ID_KEY_G:
			case Block::ID_KEY_B:
				{
					int key_id = b.id - Block::ID_KEY_R;
					auto &kdata = meta.keys[key_id];
					if (kdata.set(5.0f)) {
						Packet out;
						out.write(Packet2Client::ActivateBlock);
						out.write(b.id);
						out.write<u8>(kdata.isActive());
						broadcastInWorld(player, 1, out);
					}
				}
				break;
			case Block::ID_SWITCH:
				meta.switch_state ^= 0x80;
				break;
			case Block::ID_CHECKPOINT:
				player->checkpoint = pos;
				break;
			case Block::ID_SPIKES:
				is_dead = true;
				break;
		}
	}

	if (is_dead) {
		if (m_deaths.find(peer_id) == m_deaths.end())
			m_deaths.insert({peer_id, Timer(1.0f) });
	}
}

void Server::pkt_ScriptEvent(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	if (player->rl_scriptevents.isActive())
		return; // ignore packet

	RateLimit &rl = player->rl_scriptevents;
	auto *smgr = m_script->getSEMgr();
	m_script->setPlayer(player);

	ScriptEvent se;
	while (smgr->readNextEvent(pkt, false, se)) {
		if (rl.add(1))
			break; // limit active. discard events.

		smgr->runLuaEventCallback(se);
	}
	m_script->setPlayer(nullptr);
}

void Server::pkt_GodMode(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	bool status = pkt.read<u8>();

	if (status) {
		if ((player->getFlags().flags & PlayerFlags::PF_GODMODE) == 0) {
			DEBUGLOG("pkt_GodMode: missing flags for %s (%08X)\n",
				player->name.c_str(), player->getFlags().flags);
			return;
		}
	}

	player->setGodMode(status);
	if (status)
		m_deaths.erase(player->peer_id);

	Packet out;
	out.write(Packet2Client::GodMode);
	out.write(peer_id);
	out.write<u8>(status);

	broadcastInWorld(player, 1, out);
}

void Server::pkt_Smiley(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	player->smiley_id = pkt.read<u8>();

	Packet out;
	out.write(Packet2Client::Smiley);
	out.write(peer_id);
	out.write<u8>(player->smiley_id);

	broadcastInWorld(player, 1, out);
}

void Server::pkt_FriendAction(peer_t peer_id, Packet &pkt)
{
	if (!m_auth_db) {
		sendMsg(peer_id, "Service unavailable");
		return;
	}

	RemotePlayer *player = getPlayerNoLock(peer_id);
	const uint8_t action = pkt.read<uint8_t>();

	if (action == (int)LobbyFriend::Type::None)
		return;

	if (player->auth.status == Auth::Status::Guest) {
		sendMsg(peer_id, "Cannot perform friend actions for guests.");
		return;
	}


	auto try_find_friend = [this] (AuthFriend *af) -> bool {
		std::vector<AuthFriend> friends;
		m_auth_db->listFriends(af->p1.name, &friends);
		for (auto &f : friends) {
			if (f.p2.name == af->p2.name) {
				*af = std::move(f);
				return true;
			}
		}
		return false;
	};

	AuthFriend af;
	af.p1.name = player->name;

	if (action == (int)LobbyFriend::Type::Accepted) {
		af.p2.name = pkt.readStr16();
		if (af.p2.name == player->name)
			return;

		bool exists = try_find_friend(&af);

		if (!exists) {
			AuthBanEntry ban;
			ban.affected = player->name;
			ban.context = "friend.send";
			if (m_auth_db->getBanRecord(ban.affected, ban.context, nullptr)) {
				sendMsg(peer_id, "Cooldown triggered. Please wait before adding someone else.");
				return;
			}
			// Add a new friend if possible
			AuthAccount auth;
			if (!m_auth_db->load(af.p2.name, &auth)) {
				sendMsg(peer_id, "The specified player was not found.");
				return;
			}
			af.p1.status = (int)LobbyFriend::Type::Accepted;
			af.p2.status = (int)LobbyFriend::Type::Pending;
			if (m_auth_db->setFriend(af)) {
				ban.expiry = time(nullptr) + 120;
				m_auth_db->ban(ban);
				sendMsg(peer_id, "Success!");
			} else {
				sendMsg(peer_id, "Failed to add friend.");
			}
			return;
		}

		// Update record
		if (af.p1.status == (int)LobbyFriend::Type::Pending) {
			af.p1.status = (int)LobbyFriend::Type::Accepted;
			if (m_auth_db->setFriend(af)) {
				sendMsg(peer_id, "Success!");
			} else {
				sendMsg(peer_id, "Failed to accept.");
			}
		} else {
			sendMsg(peer_id, "You already accepted.");
		}
		return;
	}

	if (action == (int)LobbyFriend::Type::Rejected) {
		af.p2.name = pkt.readStr16();
		bool exists = try_find_friend(&af);

		if (!exists) {
			sendMsg(peer_id, "No such friend.");
			return;
		}

		if (m_auth_db->removeFriend(af.p1.name, af.p2.name)) {
			sendMsg(peer_id, "Success!");
		} else {
			sendMsg(peer_id, "Failed to remove.");
		}
		return;
	}

	sendMsg(peer_id, "Unknown friend action: " + std::to_string(action));
}

void Server::pkt_Deprecated(peer_t peer_id, Packet &pkt)
{
	std::string name = "??";

	auto player = getPlayerNoLock(peer_id);
	if (player)
		name = player->name;

	printf("Server: Ignoring deprecated packet from player %s, peer_id=%u\n",
		name.c_str(), peer_id);
}

void Server::sendMsg(peer_t peer_id, const std::string &text)
{
	Packet pkt;
	pkt.write<Packet2Client>(Packet2Client::Message);
	pkt.writeStr16(text);
	m_con->send(peer_id, 0, pkt);
}

void Server::sendPlayerLeave(RemotePlayer *player)
{
	if (m_script)
		m_script->onPlayerEvent("leave", player);

	Packet out;
	out.write(Packet2Client::Leave);
	out.write(player->peer_id);
	broadcastInWorld(player, 0, out);

	player->setWorld(nullptr);
	player->state = RemotePlayerState::Idle;
}

void Server::broadcastInWorld(Player *player, int flags, Packet &pkt)
{
	if (!player)
		return;

	auto world = player->getWorld();
	broadcastInWorld(world.get(), flags, pkt);
}

void Server::broadcastInWorld(const World *world, int flags, Packet &pkt)
{
	if (!world)
		return;

	// Send to all players within this world
	for (auto &p : m_players) {
		if (p.second->getWorld().get() != world)
			continue;

		m_con->send(p.first, flags, pkt);
	}
}

void Server::broadcastInWorld(Player *player, RemotePlayerState min_state,
	int flags, std::function<void(Packet &)> cb)
{
	if (!player)
		return;

	auto world = player->getWorld();
	if (!world)
		return;

	std::map<u16, Packet> compat;

	// Send to all players within this world
	for (auto &it : m_players) {
		auto p = (RemotePlayer *)it.second.get();
		if (p->getWorld() != world)
			continue;

		// Player is yet not ready
		if ((int)p->state < (int)min_state)
			continue;

		u16 proto_ver = p->protocol_version;

		// Creates a new instance if needed
		auto it_pkt = compat.find(proto_ver);
		Packet *pkt = nullptr;
		if (it_pkt == compat.end()) {
			pkt = &compat[proto_ver];
			pkt->data_version = proto_ver;
			cb(*pkt);
		} else {
			pkt = &it_pkt->second;
		}

		// No relevant data
		if (pkt->size() <= sizeof(Packet2Client))
			continue;

		m_con->send(it.first, flags, *pkt);
		DEBUGLOG("broadcastInWorld: send after cb. name=%s, ver=%d\n",
			it.second->name.c_str(), p->protocol_version);
	}

#if 0
	// EXAMPLE

	broadcastInWorld(player, RemotePlayerState::WorldJoin, 1, SERVER_PKT_CB {
		if (proto_ver < 4)
			return;

		out.write<u16>(4324);
	});
#endif
}
