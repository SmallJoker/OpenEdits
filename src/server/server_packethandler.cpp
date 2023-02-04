#include "server.h"
#include "remoteplayer.h"
#include "core/packet.h"
#include "core/world.h"

// in sync with core/packet.h
const ServerPacketHandler Server::packet_actions[] = {
	{ RemotePlayerState::Invalid,   &Server::pkt_Quack }, // 0
	{ RemotePlayerState::Invalid,   &Server::pkt_Hello },
	{ RemotePlayerState::Idle,      &Server::pkt_GetLobby },
	{ RemotePlayerState::Idle,      &Server::pkt_Join },
	{ RemotePlayerState::WorldJoin, &Server::pkt_Leave },
	{ RemotePlayerState::WorldPlay, &Server::pkt_Move }, // 5
	{ RemotePlayerState::WorldPlay, &Server::pkt_Chat },
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
		printf("Old peer_id=%u tried to connect\n", peer_id);
		m_con->disconnect(peer_id);
		return;
	}

	std::string name(pkt.readStr16());
	for (char &c : name)
		c = tolower(c);

	bool ok = true;
	for (auto it : m_players) {
		if (it.second->name == name) {
			ok = false;
			break;
		}
	}

	if (!ok) {
		sendError(peer_id, "Player is already online");
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

		m_con->send(peer_id, 0, reply);
	}

	printf("Hello from %s, proto_ver=%d\n", player->name.c_str(), player->protocol_version);
}

void Server::pkt_GetLobby(peer_t peer_id, Packet &)
{
	Packet out;
	out.write(Packet2Client::Lobby);

	for (auto it : m_worlds) {
		auto meta = it.second->getMeta();
		if (!meta.is_public)
			continue;

		out.write<u8>(true); // continue!

		out.writeStr16(it.first); // world ID
		blockpos_t size = it.second->getSize();
		out.write(size.X);
		out.write(size.Y);
		out.writeStr16(meta.title);
		out.writeStr16(meta.owner);
		out.write<u16>(meta.online);
		out.write<u32>(meta.plays);
	}
	out.write<u8>(false); // terminate

	m_con->send(peer_id, 0, out);
}

void Server::pkt_Join(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayer(peer_id);
	std::string world_id(pkt.readStr16());

	// query database for existing world
	auto it = m_worlds.find(world_id);
	if (it == m_worlds.end()) {
		World *world = new World();
		world->createDummy({10, 10});
		auto ret = m_worlds.emplace(world_id, world);

		// ?? we already checked for it
		if (!ret.second)
			delete world;

		it = ret.first;
	}

	Packet out;
	out.write(Packet2Client::WorldData);
	out.write<u8>(1); // status indicator

	World *world = it->second;

	{
		// Update player information
		player->joinWorld(world);
		player->state = RemotePlayerState::WorldPlay;
	}

	blockpos_t size = world->getSize();
	out.write(size.X); // dimensions
	out.write(size.Y);

	for (size_t y = 0; y < size.Y; ++y)
	for (size_t x = 0; x < size.X; ++x) {
		Block b;
		world->getBlock(blockpos_t(x, y), &b);
		out.write(b.id);
	}

	out.write<u8>(0xF8); // validation

	m_con->send(peer_id, 0, out);


	// Notify about new player
	Packet pkt_new;
	pkt_new.write(Packet2Client::Join);
	pkt_new.write(peer_id);
	pkt_new.writeStr16(player->name);
	player->writePhysics(pkt_new);

	for (auto it : m_players) {
		RemotePlayer *p = dynamic_cast<RemotePlayer *>(it.second);
		if (p->getWorld() != world)
			continue;

		// Notify existing players about the new one
		m_con->send(it.first, 0, pkt_new);

		// Notify new player about existing ones
		Packet out;
		out.write(Packet2Client::Join);
		out.write(p->peer_id);
		out.writeStr16(p->name);
		p->writePhysics(out);
		m_con->send(peer_id, 0, out);
	}
}

void Server::pkt_Leave(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayer(peer_id);
	World *world = player->getWorld();
	if (!world)
		return; // ???

	Packet out;
	out.write(Packet2Client::Leave);
	out.write(peer_id);

	broadcastInWorld(player, 0, out);

	player->leaveWorld();
	player->state = RemotePlayerState::Idle;
}

void Server::pkt_Move(peer_t peer_id, Packet &pkt)
{
	RemotePlayer *player = getPlayer(peer_id);
	ASSERT_FORCED(player, "Player required!");

	// TODO: Anticheat test here
	core::vector2df
		pos = player->pos,
		vel = player->vel,
		acc = player->acc;

	player->readPhysics(pkt);


	// broadcast to connected players
	Packet out;
	out.write(Packet2Client::Move);
	out.write<u8>(true); // start
	{
		// Bulk sending (future)
		out.write(peer_id);
		player->writePhysics(out);
	}
	out.write<u8>(false); // stop

	broadcastInWorld(player, Connection::FLAG_UNRELIABLE, out);
}

void Server::pkt_Chat(peer_t peer_id, Packet &pkt)
{
	std::string message(pkt.readStr16());
	if (message.size() > 200)
		message.resize(200);

	bool ok = true;
	for (char c : message) {
		if (c < ' ') {
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

	RemotePlayer *player = getPlayer(peer_id);

	Packet out;
	out.write(Packet2Client::Chat);
	out.write(peer_id);
	out.writeStr16(message);

	broadcastInWorld(player, 1, out);
}

void Server::pkt_Deprecated(peer_t peer_id, Packet &pkt)
{
	std::string name = "??";

	auto player = getPlayer(peer_id);
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
	World *world = player->getWorld();
	if (!world)
		return;

	// Send to all players within this world
	for (auto it : m_players) {
		if (it.second->getWorld() != world)
			continue;

		m_con->send(it.first, flags, pkt);
	}
}
