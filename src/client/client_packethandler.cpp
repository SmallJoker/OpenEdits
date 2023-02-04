#include "client.h"
#include "localplayer.h"
#include "core/packet.h"

const ClientPacketHandler Client::packet_actions[] = {
	{ false, &Client::pkt_Quack }, // 0
	{ false, &Client::pkt_Hello },
	{ false, &Client::pkt_Error },
	{ false, &Client::pkt_Lobby },
	{ true,  &Client::pkt_WorldData },
	{ true,  &Client::pkt_Join }, // 5
	{ true,  &Client::pkt_Leave },
	{ true,  &Client::pkt_Move },
	{ true,  &Client::pkt_Chat },
	{ false, 0 }
};

void Client::pkt_Quack(Packet &pkt)
{
	printf("Client: Got packet %s\n", pkt.dump().c_str());
}

void Client::pkt_Hello(Packet &pkt)
{
	m_protocol_version = pkt.read<uint16_t>();
	m_my_peer_id = pkt.read<peer_t>();

	m_state = ClientState::LobbyIdle;
	printf("Client: hello. my peer_id=%d\n", m_my_peer_id);
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
		world.size.X = pkt.read<u16>();
		world.size.Y = pkt.read<u16>();
		world.title = pkt.readStr16();
		world.owner = pkt.readStr16();
		world.online = pkt.read<u16>();
		world.plays = pkt.read<u32>();

		world_list.emplace(world_id, world);
	}
}

void Client::pkt_WorldData(Packet &pkt)
{
	bool is_ok = pkt.read<u8>();
	if (!is_ok) {
		// back to lobby
		GameEvent e(GameEvent::C2G_PLAYER_LEAVE);
		e.player = nullptr;
		sendNewEvent(e);
		return;
	}

	if (m_world)
		delete m_world;

	m_world = new World();

	blockpos_t size;
	pkt.read<u16>(size.X);
	pkt.read<u16>(size.Y);
	m_world->createDummy(size);

	for (size_t y = 0; y < size.Y; ++y)
	for (size_t x = 0; x < size.X; ++x) {
		Block b;
		pkt.read(b.id);
		m_world->setBlock(blockpos_t(x, y), b);
	}

	if (pkt.read<u8>() != 0xF8) {
		fprintf(stderr, "Client: ERROR while reading world data!\n");
	}

	{
		m_state = ClientState::WorldPlay;

		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
	}
}

void Client::pkt_Join(Packet &pkt)
{
	peer_t peer_id = pkt.read<peer_t>();

	LocalPlayer *player = getPlayer(peer_id);
	if (!player) {
		// normal case. player should yet not exist!
		m_players.emplace(peer_id, new LocalPlayer(peer_id));
	}
	player = getPlayer(peer_id);

	player->name = pkt.readStr16();
	player->readPhysics(pkt);

	GameEvent e(GameEvent::C2G_PLAYER_JOIN);
	e.player = player;
	sendNewEvent(e);
}

void Client::pkt_Leave(Packet &pkt)
{
	peer_t peer_id = pkt.read<peer_t>();
	LocalPlayer *player = getPlayer(peer_id);

	m_players.erase(peer_id);

	GameEvent e(GameEvent::C2G_PLAYER_LEAVE);
	e.player = player;
	sendNewEvent(e);

	delete player;

	if (peer_id == m_my_peer_id) {
		for (auto it : m_players) {
			delete it.second;
		}
		m_players.clear();

		m_state = ClientState::LobbyIdle;
	}

}

void Client::pkt_Move(Packet &pkt)
{
	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		peer_t peer_id = pkt.read<peer_t>();
		LocalPlayer *player = getPlayer(peer_id);
		if (!player || peer_id == m_my_peer_id) {
			// don't care
			continue;
		}

		// maybe do interpolation?
		player->readPhysics(pkt);
	}
}

void Client::pkt_Chat(Packet &pkt)
{
	peer_t peer_id = pkt.read<peer_t>();
	LocalPlayer *player = getPlayer(peer_id);
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

void Client::pkt_Deprecated(Packet &pkt)
{
	printf("Ignoring deprecated packet %s\n", pkt.dump().c_str());
}
