#include "server.h"
#include "remoteplayer.h"
#include "core/blockmanager.h"
#include "core/packet.h"
#include "core/utils.h"
#include "core/world.h"
#include "server/database_auth.h"
#include "server/database_world.h"
#include <set>

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
	{ RemotePlayerState::Invalid, 0 }
};


void Server::pkt_Quack(peer_t peer_id, Packet &pkt)
{
	printf("Server: Got %zu bytes from peer_id=%u\n", pkt.size(), peer_id);
}

void Server::pkt_Hello(peer_t peer_id, Packet &pkt)
{
	uint16_t protocol_ver = pkt.read<uint16_t>();
	uint16_t protocol_min = pkt.read<uint16_t>();

	protocol_ver = std::min(PROTOCOL_VERSION, protocol_ver);
	if (protocol_ver < protocol_min || protocol_ver < PROTOCOL_VERSION_MIN) {
		char buf[255];
		snprintf(buf, sizeof(buf), "server=[%d,%d], client=[%d,%d]",
			protocol_min, protocol_ver, PROTOCOL_VERSION_MIN, PROTOCOL_VERSION);

		printf("Protocol mismatch. peer_id=%u tried to connect: %s\n", peer_id, buf);
		sendMsg(peer_id, std::string("Incompatible protocol versions. ") + buf);
		m_con->disconnect(peer_id);
		return;
	}

	std::string name(pkt.readStr16());
	name = strtrim(name);

	bool ok = name.size() <= 16 && name.size() >= 3 && isalnum_nolocale(name);
	for (char &c : name) {
		c = toupper(c);
	}

	for (auto it : m_players) {
		if (it.second->name == name) {
			ok = false;
			break;
		}
	}

	if (!ok) {
		sendMsg(peer_id, "Invalid nickname (must be [A-z0-9]{3,16}) or player is already online");
		m_con->disconnect(peer_id);
		return;
	}

	auto player = new RemotePlayer(peer_id, protocol_ver);
	m_players.emplace(peer_id, player);

	player->name = name;
	player->state = RemotePlayerState::Login;

	{
		// Confirm
		Packet reply;
		reply.write(Packet2Client::Hello);
		reply.write(player->protocol_version);
		reply.write(player->peer_id);
		reply.writeStr16(player->name);

		m_bmgr->write(reply, player->protocol_version);

		m_con->send(peer_id, 0, reply);
	}

	printf("Server: Hello from %s, proto_ver=%d\n", player->name.c_str(), player->protocol_version);

	{
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
		}

		if (is_guest) {
			player->auth.status = Auth::Status::Guest;
			player->state = RemotePlayerState::Idle;

			// Log in instantly
			Packet out;
			out.write(Packet2Client::Auth);
			out.writeStr16("signed_in");
			m_con->send(peer_id, 0, out);
			return;
		}

		// Prompt for authentication if needed
		if (m_auth_db) {
			AuthInformation info;
			bool ok = m_auth_db->load(player->name, &info);
			if (ok) {
				player->auth.random = Auth::generateRandom();

				Packet out;
				out.write(Packet2Client::Auth);
				out.writeStr16("hash");
				out.writeStr16(player->auth.random);
				m_con->send(peer_id, 0, out);
			} else {
				player->auth.status = Auth::Status::Unregistered;

				Packet out;
				out.write(Packet2Client::Auth);
				out.writeStr16("register");
				m_con->send(peer_id, 0, out);
			}
		}
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

	if (action == "hash") {
		// Confirm the client-sent hash

		AuthInformation info;
		bool ok = m_auth_db->load(player->name, &info);
		if (!ok) {
			sendMsg(peer_id, "Invalid action");
			return;
		}

		std::string hash = pkt.readStr16();

		bool signed_in = false;
		if (!info.password.empty()) {
			// Try normal password
			player->auth.fromHash(info.password);
			player->auth.combine(player->auth.random);
			signed_in = player->auth.verify(hash);
		}

		if (!signed_in && !info.password_reset.empty()) {
			// Try temporary reset password
			player->auth.fromHash(info.password_reset);
			player->auth.combine(player->auth.random);
			signed_in = player->auth.verify(hash);
		}

		if (!signed_in) {
			sendMsg(peer_id, "Incorrect password");
			m_con->disconnect(peer_id);
			return;
		}

		player->auth.status = Auth::Status::SignedIn;
		player->state = RemotePlayerState::Idle;

		Packet out;
		out.write(Packet2Client::Auth);
		out.writeStr16("signed_in");
		m_con->send(peer_id, 0, out);

		// Update last login timestamp
		info.last_login = time(nullptr);
		m_auth_db->save(info);
		return;
	}

	if (action == "register") {
		// Register based on email

		AuthInformation info;
		bool ok = m_auth_db->load(player->name, &info);
		if (ok) {
			sendMsg(peer_id, "This user is already registered.");
			return;
		}

		info.name = player->name;
		info.email = strtrim(pkt.readStr16());

		bool email_ok = true;
		bool email_at = false;
		for (char c : info.email) {
			if (!std::isgraph(c)) {
				email_ok = false;
				break;
			}
			if (c == '@')
				email_at = true;
		}
		email_ok &= email_at;
		if (!email_ok) {
			sendMsg(peer_id, "Invalid email. Please use one where you can receive your temporary password.");
			return;
		}

		{
			// Check email use
			AuthInformation info_tmp;
			ok = m_auth_db->load(info.email, &info_tmp);
			if (ok) {
				sendMsg(peer_id, "This email is already in use.");
				return;
			}
		}

		m_auth_db->save(info);
		m_auth_db->resetPassword(info.email);
		sendMsg(peer_id, "Account registered. Check your email for the first time password.");

		AuthLogEntry log;
		log.subject = player->name;
		log.text = "registered";
		m_auth_db->logNow(log);
		return;
	}

	if (action == "setpass" && player->auth.status == Auth::Status::SignedIn) {
		// Update password with specified hash
		AuthInformation info;
		bool ok = m_auth_db->load(player->name, &info);
		if (!ok) {
			sendMsg(peer_id, "Cannot change password: Auth not found.");
			return;
		}

		info.password = pkt.readStr16(); // new hash
		info.password_reset.clear();

		ok = m_auth_db->save(info);
		if (ok)
			sendMsg(peer_id, "Password updated.");
		else
			sendMsg(peer_id, "Failed to update password.");
		return;
	}

	if (action == "resetpass") {
		// Send new password by email
		AuthInformation info;
		bool ok = m_auth_db->load(player->name, &info);
		if (!ok) {
			sendMsg(peer_id, "Cannot reset password: Auth not found.");
			return;
		}

		ok = m_auth_db->resetPassword(info.email);
		if (ok)
			sendMsg(peer_id, "New password sent! Check your inbox.");
		else
			sendMsg(peer_id, "Cannot reset your password. Ask a system admin.");
		return;
	}

	sendMsg(peer_id, "Unknown auth action: " + action);
}

void Server::pkt_GetLobby(peer_t peer_id, Packet &)
{
	Packet out;
	out.write(Packet2Client::Lobby);

	std::set<World *> worlds;
	for (auto p : m_players) {
		auto world = p.second->getWorld();
		if (world)
			worlds.insert(world);
	}

	for (auto world : worlds) {
		const auto &meta = world->getMeta();
		if (!meta.is_public)
			continue;

		out.write<u8>(true); // continue!

		out.writeStr16(meta.id); // world ID
		meta.writeCommon(out);
		// Additional Lobby fields
		blockpos_t size = world->getSize();
		out.write(size.X);
		out.write(size.Y);
	}

	if (m_world_db) {
		auto player = getPlayerNoLock(peer_id);
		auto found = m_world_db->getByPlayer(player->name);
		for (const auto &meta : found) {
			out.write<u8>(true); // continue!

			out.writeStr16(meta.id); // world ID
			meta.writeCommon(out);
			// Additional Lobby fields
			out.write(meta.size.X);
			out.write(meta.size.Y);
		}
	}

	out.write<u8>(false); // terminate

	m_con->send(peer_id, 0, out);
}

void Server::pkt_Join(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	std::string world_id(pkt.readStr16());
	world_id = strtrim(world_id);

	{
		bool is_ok = !world_id.empty() && isalnum_nolocale(world_id);

		if (!is_ok) {
			sendMsg(peer_id, "Invalid world ID. [A-z0-9]+ are allowed");
			return;
		}
	}

	// query database for existing world
	auto world = getWorldNoLock(world_id);
	if (!world && m_world_db) {
		// try to load from the database
		world = new World(m_bmgr, world_id);
		world->drop(); // kept alive by RefCnt

		bool found = m_world_db->load(world.ptr());
		if (!found) {
			world = nullptr;
		}
	}
	if (!world) {
		// create a new one
		world = new World(m_bmgr, world_id);
		world->drop(); // kept alive by RefCnt

		world->createDummy({100, 75});
		world->getMeta().owner = player->name;
	}

	{
		// Ban check
		PlayerFlags pflags = world->getMeta().getPlayerFlags(player->name);
		if (pflags.flags & (PlayerFlags::PF_BANNED | PlayerFlags::PF_TMP_HEAVYKICK)) {
			sendMsg(peer_id, "You may not enter this world. Reason: kicked or banned.");
			return;
		}
	}

	{
		Packet out;
		writeWorldData(out, *world.ptr(), false);
		m_con->send(peer_id, 0, out);
	}

	{
		// Update player information
		player->setWorld(world.ptr());
		respawnPlayer(player, false);
		player->state = RemotePlayerState::WorldPlay;
	}

	// Notify about new player
	Packet pkt_new;
	pkt_new.write(Packet2Client::Join);
	pkt_new.write(peer_id);
	pkt_new.writeStr16(player->name);
	pkt_new.write<u8>(player->godmode);
	pkt_new.write<u8>(player->smiley_id);
	player->writePhysics(pkt_new);

	for (auto it : m_players) {
		auto *p = it.second;
		if (p->getWorld() != world.ptr())
			continue;

		// Notify existing players about the new one
		m_con->send(it.first, 0, pkt_new);

		// Notify new player about existing ones
		Packet out;
		out.write(Packet2Client::Join);
		out.write(p->peer_id);
		out.writeStr16(p->name);
		out.write<u8>(p->godmode);
		out.write<u8>(p->smiley_id);
		p->writePhysics(out);
		m_con->send(peer_id, 0, out);
	}

	{
		// Set player flags
		Packet out;
		out.write(Packet2Client::PlayerFlags);
		out.write<playerflags_t>(player->getFlags().flags); // new flags
		out.write<playerflags_t>(PlayerFlags::PF_MASK_SEND_PLAYER); // mask
		m_con->send(peer_id, 0, out);
	}

	printf("Server: Player %s joined\n", player->name.c_str());
}

void Server::pkt_Leave(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	Packet out;
	out.write(Packet2Client::Leave);
	out.write(peer_id);

	broadcastInWorld(player, 0, out);

	player->setWorld(nullptr);
	player->state = RemotePlayerState::Idle;
}

void Server::pkt_Move(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	ASSERT_FORCED(player, "Player required!");

	// TODO: Anticheat test here
	/*core::vector2df
		pos = player->pos,
		vel = player->vel,
		acc = player->acc;*/

	player->readPhysics(pkt);

	// broadcast to connected players
	Packet out;
	out.write(Packet2Client::Move);
	{
		// Bulk sending (future)
		out.write(peer_id);
		player->writePhysics(out);
	}
	out.write<peer_t>(0); // end of bulk

	broadcastInWorld(player, 1 | Connection::FLAG_UNRELIABLE, out);
}

void Server::pkt_Chat(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	if (player->getFlags().check(PlayerFlags::PF_TMP_MUTED))
		return;

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

	if (m_chatcmd.run(player, message))
		return; // Handled

	if (message[0] == '/') {
		// Unknown command
		systemChatSend(player, "Unknown command. See /help");
		return;
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
	if ((pflags.flags & PlayerFlags::PF_MASK_EDIT_DRAW) == 0)
		return; // Missing permissions
	// TODO: Add cooldown check for non-draw users

	auto world = player->getWorld();
	SimpleLock lock(world->mutex);

	BlockUpdate bu(world->getBlockMgr());
	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		bu.peer_id = peer_id;
		bu.read(pkt);

		bool ok = world->updateBlock(bu);
		if (!ok) {
			// out of range?
			continue;
		}

		// Put into queue to keep the world lock as short as possible
		world->proc_queue.insert(bu);
	}
}

void Server::pkt_TriggerBlocks(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);
	auto world = player->getWorld();
	auto &meta = world->getMeta();

	// TODO: CHeck if the player is nearby
	while (true) {
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
					if (kdata.trigger(5.0f)) {
						Packet out;
						out.write(Packet2Client::Key);
						out.write(b.id);
						out.write<u8>(kdata.active);
						broadcastInWorld(player, 1, out);
					}
				}
				break;
		}
	}
}

void Server::pkt_GodMode(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	bool status = pkt.read<u8>();

	if (status) {
		if ((player->getFlags().flags & PlayerFlags::PF_MASK_GODMODE) == 0)
			return;
	}

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

void Server::pkt_Deprecated(peer_t peer_id, Packet &pkt)
{
	std::string name = "??";

	auto player = getPlayerNoLock(peer_id);
	if (player)
		name = player->name;

	printf("Ignoring deprecated packet from player %s, peer_id=%u\n",
		name.c_str(), peer_id);
}

void Server::sendMsg(peer_t peer_id, const std::string &text)
{
	Packet pkt;
	pkt.write<Packet2Client>(Packet2Client::Message);
	pkt.writeStr16(text);
	m_con->send(peer_id, 0, pkt);
}

void Server::broadcastInWorld(Player *player, int flags, Packet &pkt)
{
	if (!player)
		return;

	auto world = player->getWorld();
	if (!world)
		return;

	// Send to all players within this world
	for (auto it : m_players) {
		if (it.second->getWorld() != world)
			continue;

		m_con->send(it.first, flags, pkt);
	}
}
