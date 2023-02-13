#include "client.h"
#include "localplayer.h"
#include "core/packet.h"

const ClientPacketHandler Client::packet_actions[] = {
	{ ClientState::None,      &Client::pkt_Quack }, // 0
	{ ClientState::None,      &Client::pkt_Hello },
	{ ClientState::None,      &Client::pkt_Error },
	{ ClientState::Connected, &Client::pkt_Lobby },
	{ ClientState::WorldJoin, &Client::pkt_WorldData },
	{ ClientState::WorldJoin, &Client::pkt_Join }, // 5
	{ ClientState::WorldJoin, &Client::pkt_Leave },
	{ ClientState::WorldPlay, &Client::pkt_Move },
	{ ClientState::WorldPlay, &Client::pkt_Chat },
	{ ClientState::WorldPlay, &Client::pkt_PlaceBlock },
	{ ClientState::Invalid, 0 }
};

void Client::pkt_Quack(Packet &pkt)
{
	printf("Client: Got packet %s\n", pkt.dump().c_str());
}

void Client::pkt_Hello(Packet &pkt)
{
	m_protocol_version = pkt.read<uint16_t>();
	m_my_peer_id = pkt.read<peer_t>();

	auto player = new LocalPlayer(m_my_peer_id);
	m_nickname = player->name = pkt.readStr16();
	m_players.emplace(m_my_peer_id, player);

	m_state = ClientState::LobbyIdle;
	printf("Client: got HELLO. my peer_id=%u\n", m_my_peer_id);
}

void Client::pkt_Error(Packet &pkt)
{
	std::string str(pkt.readStr16());
	printf("Client received error: %s\n", str.c_str());
}

void Client::pkt_Lobby(Packet &pkt)
{
	world_list.clear();
	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		std::string world_id(pkt.readStr16());

		LobbyWorld world;
		world.readCommon(pkt);
		world.size.X = pkt.read<u16>();
		world.size.Y = pkt.read<u16>();

		world_list.emplace(world_id, world);
	}

	{
		GameEvent e(GameEvent::C2G_LOBBY_UPDATE);
		sendNewEvent(e);
	}
}

void Client::pkt_WorldData(Packet &pkt)
{
	bool is_ok = pkt.read<u8>();
	if (!is_ok) {
		m_state = ClientState::LobbyIdle;

		// back to lobby
		GameEvent e(GameEvent::C2G_LEAVE);
		sendNewEvent(e);
		return;
	}

	RefCnt<World> world(new World(m_world_id));
	world->drop(); // kept alive by RefCnt

	world->getMeta().readCommon(pkt);
	blockpos_t size;
	pkt.read<u16>(size.X);
	pkt.read<u16>(size.Y);
	world->createDummy(size);

	for (size_t y = 0; y < size.Y; ++y)
	for (size_t x = 0; x < size.X; ++x) {
		Block b;
		pkt.read(b.id);
		pkt.read(b.bg);
		world->setBlock(blockpos_t(x, y), b);
	}

	if (pkt.read<u8>() != 0xF8) {
		fprintf(stderr, "Client: ERROR while reading world data!\n");
	}

	SimpleLock lock(m_players_lock);
	Player *player = getPlayerNoLock(m_my_peer_id);
	if (!player) {
		auto ret = m_players.emplace(m_my_peer_id, new LocalPlayer(m_my_peer_id));
		player = ret.first->second;
	}

	// World kept alive by at least one player
	player->setWorld(world.ptr());

	{
		m_state = ClientState::WorldPlay;

		GameEvent e(GameEvent::C2G_JOIN);
		sendNewEvent(e);
	}
}

void Client::pkt_Join(Packet &pkt)
{
	SimpleLock lock(m_players_lock);

	peer_t peer_id = pkt.read<peer_t>();
	LocalPlayer *player = getPlayerNoLock(peer_id);

	if (!player) {
		// normal case. player should yet not exist!
		m_players.emplace(peer_id, new LocalPlayer(peer_id));
	}
	player = getPlayerNoLock(peer_id);
	player->setWorld(getWorld().ptr());

	player->name = pkt.readStr16();
	player->readPhysics(pkt);

	{
		GameEvent e(GameEvent::C2G_PLAYER_JOIN);
		e.player = player;
		sendNewEvent(e);
	}
}

void Client::pkt_Leave(Packet &pkt)
{
	SimpleLock lock(m_players_lock);

	peer_t peer_id = pkt.read<peer_t>();
	LocalPlayer *player = getPlayerNoLock(peer_id);

	m_players.erase(peer_id);

	{
		GameEvent e(GameEvent::C2G_PLAYER_LEAVE);
		e.player = player;
		sendNewEvent(e);
	}

	delete player;

	if (peer_id == m_my_peer_id) {
		for (auto it : m_players) {
			delete it.second;
		}
		m_players.clear();

		m_state = ClientState::LobbyIdle;

		GameEvent e(GameEvent::C2G_LEAVE);
		sendNewEvent(e);
	}

}

void Client::pkt_Move(Packet &pkt)
{
	SimpleLock lock(m_players_lock);
	LocalPlayer dummy(0);

	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		peer_t peer_id = pkt.read<peer_t>();
		LocalPlayer *player = getPlayerNoLock(peer_id);
		if (!player || peer_id == m_my_peer_id) {
			// don't care
			player = &dummy;
		}

		// maybe do interpolation?
		player->readPhysics(pkt);
	}
}

void Client::pkt_Chat(Packet &pkt)
{
	SimpleLock lock(m_players_lock);

	peer_t peer_id = pkt.read<peer_t>();
	LocalPlayer *player = getPlayerNoLock(peer_id);
	if (!player)
		return;

	std::string message(pkt.readStr16());

	GameEvent e(GameEvent::C2G_PLAYER_CHAT);
	e.player_chat = new GameEvent::PlayerChat {
		.player = player,
		.message = message
	};
	sendNewEvent(e);
}

void Client::pkt_PlaceBlock(Packet &pkt)
{
	auto world = getWorld();
	if (!world)
		throw std::runtime_error("Got block but the world is not ready");

	SimpleLock lock(world->mutex);

	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		BlockUpdate bu;
		pkt.read(bu.peer_id); // ... nothing to do with this?
		pkt.read(bu.pos.X);
		pkt.read(bu.pos.Y);
		pkt.read(bu.id);

		world->updateBlock(bu);
	}

	lock.unlock();

	GameEvent e(GameEvent::C2G_MAP_UPDATE);
	sendNewEvent(e);
}


void Client::pkt_Deprecated(Packet &pkt)
{
	printf("Ignoring deprecated packet %s\n", pkt.dump().c_str());
}
