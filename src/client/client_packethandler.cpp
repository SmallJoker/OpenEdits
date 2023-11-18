#include "client.h"
#include "localplayer.h"
#include "core/auth.h"
#include "core/blockmanager.h"
#include "core/packet.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

const ClientPacketHandler Client::packet_actions[] = {
	{ ClientState::None,      &Client::pkt_Quack }, // 0
	{ ClientState::None,      &Client::pkt_Hello },
	{ ClientState::None,      &Client::pkt_Message },
	{ ClientState::None,      &Client::pkt_Auth },
	{ ClientState::Connected, &Client::pkt_Lobby },
	{ ClientState::WorldJoin, &Client::pkt_WorldData }, // 5
	{ ClientState::WorldJoin, &Client::pkt_Join },
	{ ClientState::WorldJoin, &Client::pkt_Leave },
	{ ClientState::WorldJoin, &Client::pkt_SetPosition },
	{ ClientState::WorldPlay, &Client::pkt_Move },
	{ ClientState::WorldPlay, &Client::pkt_Chat }, // 10
	{ ClientState::WorldPlay, &Client::pkt_PlaceBlock },
	{ ClientState::WorldPlay, &Client::pkt_Key },
	{ ClientState::WorldPlay, &Client::pkt_GodMode },
	{ ClientState::WorldPlay, &Client::pkt_Smiley },
	{ ClientState::WorldPlay, &Client::pkt_PlayerFlags }, // 15
	{ ClientState::WorldPlay, &Client::pkt_WorldMeta },
	{ ClientState::WorldPlay, &Client::pkt_ChatReplay },
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

	pkt.data_version = m_protocol_version; // for bmgr->read

	auto player = new LocalPlayer(m_my_peer_id);
	m_start_data.nickname = player->name = pkt.readStr16();
	m_players.emplace(m_my_peer_id, player);

	// Load the server's block properties
	m_bmgr->read(pkt);

	m_auth = Auth();

	printf("Client: got HELLO. my peer_id=%u\n", m_my_peer_id);
}

void Client::pkt_Message(Packet &pkt)
{
	std::string str(pkt.readStr16());
	printf("Client received message: %s\n", str.c_str());

	GameEvent e(GameEvent::C2G_DIALOG);
	e.text = new std::string(str);
	sendNewEvent(e);
}

void Client::pkt_Auth(Packet &pkt)
{
	std::string action = pkt.readStr16();

	if (action == "login1") {
		// Confirm password

		m_auth.salt_1_const = pkt.readStr16();
		m_auth.hash(m_auth.salt_1_const, m_start_data.password);
		m_start_data.password.clear();

		auto random = pkt.readStr16();
		m_auth.rehash(random);

		// Go ahead to the next step
		Packet out;
		out.write(Packet2Server::Auth);
		out.writeStr16("login2");
		out.writeStr16(m_auth.output);
		m_con->send(0, 0, out);

		return;
	}

	if (action == "register") {
		// Requesting a signup
		m_auth.salt_1_const = pkt.readStr16();

		m_state = ClientState::Register;
		return;
	}

	if (action == "signed_in") {
		m_state = ClientState::LobbyIdle;
		return;
	}

	if (action == "pass_set") {
		GameEvent e(GameEvent::C2G_CHANGE_PASS);
		sendNewEvent(e);
		return;
	}

	printf("Client: Unknown auth action: %s\n", action.c_str());
}

void Client::pkt_Lobby(Packet &pkt)
{
	world_list.clear();
	while (pkt.getRemainingBytes()) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		LobbyWorld world;
		world.readCommon(pkt);
		world.size.X = pkt.read<u16>();
		world.size.Y = pkt.read<u16>();

		world_list.emplace_back(world);
	}

	// Friends
	friend_list.clear();
	while (pkt.getRemainingBytes()) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		LobbyFriend f;
		f.type = (LobbyFriend::Type)pkt.read<u8>();
		f.name = pkt.readStr16();
		f.world_id = pkt.readStr16();

		friend_list.emplace_back(f);
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

	SimpleLock lock(m_players_lock);
	Player *player = getPlayerNoLock(m_my_peer_id);
	if (!player) {
		auto ret = m_players.emplace(m_my_peer_id, new LocalPlayer(m_my_peer_id));
		player = ret.first->second;
	}
	auto world_old = player->getWorld();

	RefCnt<World> world;
	if (world_old) {
		// Already joined
		world = std::make_shared<World>(world_old.get());
	} else {
		world = std::make_shared<World>(m_bmgr, m_world_id);
	}

	world->getMeta().readCommon(pkt);
	blockpos_t size;
	pkt.read<u16>(size.X);
	pkt.read<u16>(size.Y);
	world->createEmpty(size);
	bool have_world_data = (mode == 1);
	if (have_world_data) {
		world->read(pkt);
	} // else: clear

	// World kept alive by at least one player
	bool is_new_join = !world_old;
	for (auto it : m_players)
		it.second->setWorld(world);

	if (have_world_data)
		updateWorld();

	if (is_new_join) {
		m_state = ClientState::WorldPlay;

		GameEvent e(GameEvent::C2G_JOIN);
		sendNewEvent(e);
	} else {
		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
		GameEvent e2(GameEvent::C2G_META_UPDATE);
		sendNewEvent(e2);
	}
	DEBUGLOG("pkt_WorldData: done.\n");
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
	player->setWorld(getWorld());

	player->name = pkt.readStr16();
	player->setGodMode(pkt.read<u8>());
	player->smiley_id = pkt.read<u8>();
	player->readPhysics(pkt);
	if (peer_id == m_my_peer_id) {
		SimpleLock lock(getWorld()->mutex);
		player->updateCoinCount(true);
	}

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

	if (player) {
		GameEvent e(GameEvent::C2G_PLAYER_LEAVE);
		e.player = player;
		sendNewEvent(e);
	}

	// HACK: keep alive until the function finishes !
	// gameplay.cpp might access the World object at the same time,
	// which somehow results in a double-free ??
	RefCnt<World> world;
	if (player)
		world = player->getWorld();

	if (peer_id == m_my_peer_id) {
		for (auto it : m_players) {
			delete it.second;
		}
		m_players.clear();

		// Keep myself in the list
		if (player) {
			m_players.emplace(peer_id, player);
			player->setWorld(nullptr);
		}

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

	bool reset_progress = pkt.read<u8>();
	bool my_player_affected = false;

	while (pkt.getRemainingBytes()) {
		peer_t peer_id = pkt.read<peer_t>();
		if (!peer_id)
			break;

		core::vector2df pos;
		pkt.read(pos.X);
		pkt.read(pos.Y);

		LocalPlayer *player = getPlayerNoLock(peer_id);
		if (player)
			player->setPosition(pos, reset_progress);

		if (peer_id == m_my_peer_id)
			my_player_affected = true;
	}

	if (!reset_progress || !my_player_affected)
		return;

	LocalPlayer *player = getPlayerNoLock(m_my_peer_id);
	auto world = player->getWorld();

	// semi-duplicate of Player::updateCoinCount
	bool need_update = false;
	for (Block *b = world->begin(); b < world->end(); ++b) {
		if (b->tile == 0)
			continue;

		auto new_tile = getBlockTile(player, b);
		if (b->tile != new_tile) {
			b->tile = new_tile;
			need_update = true;
		}
	}

	updateWorld();

	if (need_update) {
		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
	}
}

void Client::pkt_Move(Packet &pkt)
{
	SimpleLock lock(m_players_lock);
	LocalPlayer dummy(0);

	while (pkt.getRemainingBytes()) {
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
		player->dtime_delay += m_con->getPeerRTT(0) / 2.0f;
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
	e.player_chat = new GameEvent::PlayerChat();
	e.player_chat->player = player;
	e.player_chat->message = message;

	sendNewEvent(e);
}

void Client::pkt_PlaceBlock(Packet &pkt)
{
	auto world = getWorld();
	if (!world)
		throw std::runtime_error("Got block but the world is not ready");

	auto player = getMyPlayer();
	SimpleLock lock(world->mutex);

	BlockUpdate bu(m_bmgr);
	while (pkt.getRemainingBytes()) {
		peer_t peer_id = pkt.read<peer_t>();
		if (!peer_id)
			break;

		bu.peer_id = peer_id; // ... nothing to do with this?
		bu.read(pkt);

		Block *b = world->updateBlock(bu);
		if (!b)
			continue;

		if (!bu.isBackground())
			b->tile = getBlockTile(player.ptr(), b);
	}

	player->updateCoinCount();

	lock.unlock();
	GameEvent e(GameEvent::C2G_MAP_UPDATE);
	sendNewEvent(e);
}

void Client::pkt_Key(Packet &pkt)
{
	auto player = getMyPlayer();

	bid_t key_bid = pkt.read<bid_t>();
	bool state = pkt.read<u8>();

	switch (key_bid) {
		case Block::ID_KEY_R:
		case Block::ID_KEY_G:
		case Block::ID_KEY_B:
			// good
			break;
		default:
			// Unknown key
			return;
	};

	uint8_t key_idx = key_bid - Block::ID_KEY_R;
	bid_t bid_door = key_idx + Block::ID_DOOR_R;
	bid_t bid_gate = key_idx + Block::ID_GATE_R;

	auto &timer = player->getWorld()->getMeta().keys[key_idx];
	timer.set(state); // 1.0f (active) or 0.0f (stopped)

	// Quick iterate
	size_t n = 0;
	auto world = player->getWorld();
	for (Block *b = world->begin(); b < world->end(); ++b) {
		if (b->id == bid_door || b->id == bid_gate) {
			b->tile = state;
			n++;
		}
	}

	if (n > 0) {
		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
	}
}

void Client::pkt_GodMode(Packet &pkt)
{
	peer_t peer_id = pkt.read<peer_t>();
	if (!peer_id)
		return;

	bool state = pkt.read<u8>();

	SimpleLock lock(m_players_lock);
	LocalPlayer *player = getPlayerNoLock(peer_id);
	if (player) {
		player->setGodMode(state);
		player->acc = core::vector2df();
	}
	if (peer_id == m_my_peer_id) {
		auto out = player->getWorld()->getBlocks(Block::ID_SECRET, [state](Block &b) -> bool {
			if (state)
				b.tile++;
			else if (b.tile)
				b.tile--;
			else
				return false;
			return true;
		});

		if (!out.empty()) {
			GameEvent e(GameEvent::C2G_MAP_UPDATE);
			sendNewEvent(e);
		}
	}

	// Allow multiple changes per packet
	if (pkt.getRemainingBytes())
		pkt_GodMode(pkt);
}

void Client::pkt_Smiley(Packet &pkt)
{
	SimpleLock lock(m_players_lock);

	peer_t peer_id = pkt.read<peer_t>();
	int smiley_id = pkt.read<u8>();

	LocalPlayer *player = getPlayerNoLock(peer_id);
	if (!player)
		return;

	player->smiley_id = smiley_id;
}

void Client::pkt_PlayerFlags(Packet &pkt)
{
	LocalPlayer dummy(0);

	while (pkt.getRemainingBytes()) {
		peer_t peer_id = pkt.read<peer_t>();
		if (!peer_id)
			break;

		auto player = getPlayerNoLock(peer_id);
		if (!player)
			player = &dummy;

		player->readFlags(pkt);

		if (player == &dummy)
			continue;

		DEBUGLOG("pkt_PlayerFlags: player=%s, flags=%08X\n", player->name.c_str(), player->getFlags().flags);
		GameEvent e(GameEvent::C2G_PLAYERFLAGS);
		e.player = player;
		sendNewEvent(e);
	}
}

void Client::pkt_WorldMeta(Packet &pkt)
{
	auto player = getMyPlayer();
	if (!player)
		return;

	auto &meta = player->getWorld()->getMeta();
	meta.readCommon(pkt);

	GameEvent e(GameEvent::C2G_META_UPDATE);
	sendNewEvent(e);
}

void Client::pkt_ChatReplay(Packet &pkt)
{
	auto player = getMyPlayer();
	if (!player)
		return;

	auto &meta = player->getWorld()->getMeta();
	while (pkt.getRemainingBytes() > 0) {
		WorldMeta::ChatHistory entry;
		entry.timestamp = pkt.read<u64>();
		if (!entry.timestamp)
			break;

		entry.name = pkt.readStr16();
		entry.message = pkt.readStr16();
		meta.chat_history.push_back(std::move(entry));
	}

	GameEvent e(GameEvent::C2G_CHAT_HISTORY);
	sendNewEvent(e);
}

void Client::pkt_Deprecated(Packet &pkt)
{
	printf("Ignoring deprecated packet %s\n", pkt.dump().c_str());
}
