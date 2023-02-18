#include "server.h"
#include "remoteplayer.h"
#include "core/packet.h"
#include "core/utils.h"
#include "core/world.h"
#include "server/database_world.h"
#include <set>

// in sync with core/packet.h
const ServerPacketHandler Server::packet_actions[] = {
	{ RemotePlayerState::Invalid,   &Server::pkt_Quack }, // 0
	{ RemotePlayerState::Invalid,   &Server::pkt_Hello },
	{ RemotePlayerState::Idle,      &Server::pkt_GetLobby },
	{ RemotePlayerState::Idle,      &Server::pkt_Join },
	{ RemotePlayerState::WorldJoin, &Server::pkt_Leave },
	{ RemotePlayerState::WorldPlay, &Server::pkt_Move }, // 5
	{ RemotePlayerState::WorldPlay, &Server::pkt_Chat },
	{ RemotePlayerState::WorldPlay, &Server::pkt_PlaceBlock },
	{ RemotePlayerState::WorldPlay, &Server::pkt_TriggerBlock },
	{ RemotePlayerState::WorldPlay, &Server::pkt_GodMode },
	{ RemotePlayerState::Invalid, 0 } // 10
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
		printf("Old peer_id=%u tried to connect\n", peer_id);
		m_con->disconnect(peer_id);
		return;
	}

	std::string name(pkt.readStr16());
	name = strtrim(name);
	if (name.size() > 30)
		name.resize(30);

	bool ok = !name.empty() && isalnum_nolocale(name);
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
		sendError(peer_id, "Invalid nickname or player is already online");
		m_con->disconnect(peer_id);
		return;
	}

	auto player = new RemotePlayer(peer_id, protocol_ver);
	m_players.emplace(peer_id, player);

	player->name = name;
	player->state = RemotePlayerState::Idle;

	{
		// Confirm
		Packet reply;
		reply.write(Packet2Client::Hello);
		reply.write(player->protocol_version);
		reply.write(player->peer_id);
		reply.writeStr16(player->name);

		m_con->send(peer_id, 0, reply);
	}

	printf("Server: Hello from %s, proto_ver=%d\n", player->name.c_str(), player->protocol_version);
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


	World demo("dummytest");
	demo.getMeta().owner = "foo mc bar";
	demo.createEmpty(blockpos_t(30, 20));
	worlds.insert(&demo);

	for (auto world : worlds) {
		auto meta = world->getMeta();
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
			Packet out;
			out.write(Packet2Client::Error);
			out.writeStr16("Invalid world ID. [A-z0-9]+ are allowed");
			m_con->send(peer_id, 0, out);
			return;
		}
	}

	// query database for existing world
	auto world = getWorldNoLock(world_id);
	if (!world && m_world_db) {
		// try to load from the database
		world = new World(world_id);
		bool found = m_world_db->load(world.ptr());
		if (!found) {
			world->drop();
			world = nullptr;
		}
	}
	if (!world) {
		// create a new one
		world = new World(world_id);
		world->createDummy({100, 75});
		world->getMeta().owner = player->name;
	}
	world->drop(); // kept alive by RefCnt

	Packet out;
	out.write(Packet2Client::WorldData);
	out.write<u8>(1); // 1: new data. 2: clear

	{
		// Update player information
		player->setWorld(world.ptr());
		respawnPlayer(player, false);
		player->state = RemotePlayerState::WorldPlay;
	}

	world->getMeta().writeCommon(out);
	blockpos_t size = world->getSize();
	out.write(size.X); // dimensions
	out.write(size.Y);
	world->write(out, World::Method::Plain);

	m_con->send(peer_id, 0, out);


	// Notify about new player
	Packet pkt_new;
	pkt_new.write(Packet2Client::Join);
	pkt_new.write(peer_id);
	pkt_new.writeStr16(player->name);
	pkt_new.write<u8>(player->godmode);
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
		p->writePhysics(out);
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
		Packet out;
		out.write(Packet2Client::Error);
		out.writeStr16("Control characters are not allowed");
		m_con->send(peer_id, 0, out);
		return;
	}

	RemotePlayer *player = getPlayerNoLock(peer_id);

	if (m_chatcmd.run(player, message))
		return; // Handled

	Packet out;
	out.write(Packet2Client::Chat);
	out.write(peer_id);
	out.writeStr16(message);

	broadcastInWorld(player, 1, out);
}

void Server::pkt_PlaceBlock(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	auto world = player->getWorld();
	SimpleLock lock(world->mutex);

	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		BlockUpdate bu;
		bu.peer_id = peer_id;
		pkt.read(bu.pos.X);
		pkt.read(bu.pos.Y);
		pkt.read(bu.id);

		bool ok = world->updateBlock(bu);
		if (!ok) {
			// out of range?
			continue;
		}

		// Put into queue to keep the world lock as short as possible
		world->proc_queue.emplace(bu);
	}
}

void Server::pkt_TriggerBlock(peer_t peer_id, Packet &pkt)
{
}

void Server::pkt_GodMode(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayerNoLock(peer_id);

	bool status = pkt.read<u8>();

	// TODO: enable permission check after implementing permissions
	if (status && false) {
		const auto &meta = player->getWorld()->getMeta();
		if ((meta.getPlayerFlags(player->name) & Player::FLAG_GODMODE) == 0)
			return;
	}

	Packet out;
	out.write(Packet2Client::GodMode);
	out.write(peer_id);
	out.write<u8>(status);

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

void Server::sendError(peer_t peer_id, const std::string &text)
{
	Packet pkt;
	pkt.write<Packet2Client>(Packet2Client::Error);
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
