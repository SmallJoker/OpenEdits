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
	{ ClientState::WorldJoin, &Client::pkt_SetPosition },
	{ ClientState::WorldPlay, &Client::pkt_Move },
	{ ClientState::WorldPlay, &Client::pkt_Chat },
	{ ClientState::WorldPlay, &Client::pkt_PlaceBlock }, // 10
	{ ClientState::WorldPlay, &Client::pkt_Key },
	{ ClientState::WorldJoin, &Client::pkt_GodMode },
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

	GameEvent e(GameEvent::C2G_DIALOG);
	e.text = new std::string(str);
	sendNewEvent(e);
}

void Client::pkt_Lobby(Packet &pkt)
{
	world_list.clear();
	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		std::string world_id(pkt.readStr16());

		LobbyWorld world(world_id);
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
	u8 mode = pkt.read<u8>();
	if (!mode) {
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
	if (mode == 1) {
		world->read(pkt);
	} // else: clear

	SimpleLock lock(m_players_lock);
	Player *player = getPlayerNoLock(m_my_peer_id);
	if (!player) {
		auto ret = m_players.emplace(m_my_peer_id, new LocalPlayer(m_my_peer_id));
		player = ret.first->second;
	}

	// World kept alive by at least one player
	bool is_new_join = player->getWorld() == nullptr;
	for (auto it : m_players)
		it.second->setWorld(world.ptr());

	if (is_new_join) {
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
	player->godmode = pkt.read<u8>();
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


	if (peer_id == m_my_peer_id) {
		for (auto it : m_players) {
			delete it.second;
		}
		m_players.clear();

		// Keep myself in the list
		m_players.emplace(peer_id, player);
		player->setWorld(nullptr);

		m_state = ClientState::LobbyIdle;

		GameEvent e(GameEvent::C2G_LEAVE);
		sendNewEvent(e);
	} else {
		delete player;
	}
}

void Client::pkt_SetPosition(Packet &pkt)
{
	SimpleLock lock(m_players_lock);

	bool is_respawn = pkt.read<u8>();
	bool my_player_affected = false;

	while (true) {
		peer_t peer_id = pkt.read<peer_t>();
		if (!peer_id)
			break;

		core::vector2df pos;
		pkt.read(pos.X);
		pkt.read(pos.Y);

		LocalPlayer *player = getPlayerNoLock(peer_id);
		if (player) {
			player->pos = pos;
			player->vel = core::vector2df();
		}

		if (peer_id == m_my_peer_id)
			my_player_affected = true;
	}

	if (!is_respawn || !my_player_affected)
		return;

	LocalPlayer *player = getPlayerNoLock(m_my_peer_id);
	auto out = player->getWorld()->getBlocks(Block::ID_SECRET, [](Block &b) -> bool {
		b.param1 = 0;
		return true;
	});

	if (!out.empty()) {
		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
	}
}

void Client::pkt_Move(Packet &pkt)
{
	SimpleLock lock(m_players_lock);
	LocalPlayer dummy(0);

	while (true) {
		peer_t peer_id = pkt.read<peer_t>();
		if (!peer_id)
			break;

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
	LocalPlayer *player = nullptr;
	if (peer_id != 0) {
		// Non-SYSTEM message
		player = getPlayerNoLock(peer_id);
		if (!player)
			return;
	}

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
		peer_t peer_id = pkt.read<peer_t>();
		if (!peer_id)
			break;

		BlockUpdate bu;
		bu.peer_id = peer_id; // ... nothing to do with this?
		pkt.read(bu.pos.X);
		pkt.read(bu.pos.Y);
		pkt.read(bu.id);

		world->updateBlock(bu);
	}

	lock.unlock();

	GameEvent e(GameEvent::C2G_MAP_UPDATE);
	sendNewEvent(e);
}

void Client::pkt_Key(Packet &pkt)
{
	SimpleLock lock(m_players_lock);
	LocalPlayer *player = getPlayerNoLock(m_my_peer_id);

	bid_t key_id = pkt.read<bid_t>();
	bool state = pkt.read<u8>();

	bid_t block_id = 0;
	switch (key_id) {
		case Block::ID_KEY_R:
		case Block::ID_KEY_G:
		case Block::ID_KEY_B:
			block_id = key_id - Block::ID_KEY_R + Block::ID_DOOR_R;
			break;
		default:
			// Unknown key
			return;
	};

	auto out = player->getWorld()->getBlocks(block_id, [state](Block &b) -> bool {
		b.param1 = state;
		return true;
	});

	if (!out.empty()) {
		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
	}
}

void Client::pkt_GodMode(Packet &pkt)
{
	SimpleLock lock(m_players_lock);

	peer_t peer_id = pkt.read<peer_t>();
	bool state = pkt.read<u8>();

	LocalPlayer *player = getPlayerNoLock(peer_id);
	if (player) {
		player->godmode = state;
		player->acc = core::vector2df();
	}
	if (peer_id == m_my_peer_id) {
		auto out = player->getWorld()->getBlocks(Block::ID_SECRET, [state](Block &b) -> bool {
			if (state)
				b.param1++;
			else if (b.param1)
				b.param1--;
			else
				return false;
			return true;
		});

		if (!out.empty()) {
			GameEvent e(GameEvent::C2G_MAP_UPDATE);
			sendNewEvent(e);
		}
	}
}

void Client::pkt_Deprecated(Packet &pkt)
{
	printf("Ignoring deprecated packet %s\n", pkt.dump().c_str());
}
