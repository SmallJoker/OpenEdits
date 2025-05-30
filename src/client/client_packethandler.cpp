#include "client.h"
#include "clientmedia.h"
#include "clientscript.h"
#include "localplayer.h"
#include "core/auth.h"
#include "core/blockmanager.h"
#include "core/logger.h"
#include "core/network_enums.h"
#include "core/packet.h"
#include "core/script/scriptevent.h"
#include "core/worldmeta.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

static Logger logger("ClientPkt", LL_DEBUG);

// In sync with Packet2Client
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
	{ ClientState::WorldPlay, &Client::pkt_ActivateBlock },
	{ ClientState::WorldPlay, &Client::pkt_GodMode },
	{ ClientState::WorldPlay, &Client::pkt_Smiley },
	{ ClientState::WorldPlay, &Client::pkt_PlayerFlags }, // 15
	{ ClientState::WorldPlay, &Client::pkt_WorldMeta },
	{ ClientState::WorldPlay, &Client::pkt_ChatReplay },
	{ ClientState::Connected, &Client::pkt_MediaList },
	{ ClientState::Connected, &Client::pkt_MediaReceive },
	{ ClientState::WorldPlay, &Client::pkt_ScriptEvent }, // 20
	{ ClientState::Invalid, 0 }
};

void Client::pkt_Quack(Packet &pkt)
{
	logger(LL_PRINT, "Quack! %s", pkt.dump().c_str());
}

void Client::pkt_Hello(Packet &pkt)
{
	m_protocol_version = pkt.read<uint16_t>();
	m_my_peer_id = pkt.read<peer_t>();

	pkt.data_version = m_protocol_version;

	auto player = std::make_unique<LocalPlayer>(m_my_peer_id);
	player->name = pkt.readStr16();
	m_start_data.nickname = player->name; // for completeness' sake

	m_players.emplace(m_my_peer_id, player.release());

	m_auth = Auth();

	logger(LL_DEBUG, "Hello. my peer_id=%u", m_my_peer_id);
}

void Client::pkt_Message(Packet &pkt)
{
	std::string str(pkt.readStr16());
	logger(LL_DEBUG, "message=%s", str.c_str());

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

		std::string challenge = pkt.readStr16();
		if (challenge.size() < 30) {
			disconnect("Login challenge is too short");
			return;
		}

		m_auth.rehash(challenge);

		// Go ahead to the next step
		Packet out;
		out.write(Packet2Server::Auth);
		out.writeStr16("login2");
		out.writeStr16(m_auth.output); // challenge result
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
		m_state = ClientState::MediaDownload;
		return;
	}

	if (action == "pass_set") {
		GameEvent e(GameEvent::C2G_CHANGE_PASS);
		sendNewEvent(e);
		return;
	}

	logger(LL_ERROR, "Unknown auth action '%s'", action.c_str());
}

void Client::pkt_MediaList(Packet &pkt)
{
	ASSERT_FORCED(m_media, "Missing ClientMedia");

	m_media->readMediaList(pkt);

	logger(LL_DEBUG, "%s: missing=%zu, done=%zu",
		__func__, m_media->countMissing(), m_media->countDone()
	);

	if (m_media->countMissing() > 0) {
		// Request the files for caching
		Packet out = createPacket(Packet2Server::MediaRequest);
		m_media->writeMediaRequest(out);
		m_con->send(0, 0, out);
	}
}

void Client::pkt_MediaReceive(Packet &pkt)
{
	ASSERT_FORCED(m_media, "Missing ClientMedia");

	m_media->readMediaData(pkt);

	size_t count_done = m_media->countDone(),
		bytes_done = m_media->bytes_done;
	logger(LL_DEBUG, "%s: have %zu/%zu (%zu/%zu bytes)",
		__func__,
		count_done, count_done + m_media->countMissing(),
		bytes_done, bytes_done + m_media->bytes_missing
	);
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
	SimpleLock lock(m_players_lock);
	LocalPlayer *player = getPlayerNoLock(m_my_peer_id);

	u8 mode = pkt.read<u8>();
	if (!mode) {
		quitToLobby(player);
		return;
	}

	ASSERT_FORCED(player, "LocalPlayer not initialized");
	auto world_old = player->getWorld();

	RefCnt<World> world;
	if (world_old) {
		// Already joined
		world = world_old;
	} else {
		world = std::make_shared<World>(m_bmgr, m_world_id);
	}

	world->getMeta().readCommon(pkt);
	world->getMeta().readSpecific(pkt);
	blockpos_t size;
	pkt.read<u16>(size.X);
	pkt.read<u16>(size.Y);
	world->createEmpty(size);
	bool have_world_data = (mode == 1);
	if (have_world_data) {
		world->read(pkt);
	} // else: clear

	// World kept alive by at least one player (-> me)
	for (auto it : m_players)
		it.second->setWorld(world);

	{
		GameEvent e(GameEvent::C2G_META_UPDATE);
		sendNewEvent(e);
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
		player = new LocalPlayer(peer_id);
		m_players.emplace(peer_id, player);
	}
	player->setWorld(getWorld());

	player->name = pkt.readStr16();
	player->setGodMode(pkt.read<u8>());
	player->smiley_id = pkt.read<u8>();
	player->readPhysics(pkt);
	if (peer_id == m_my_peer_id) {
		SimpleLock lock(getWorld()->mutex);
		player->updateCoinCount(true);
	}

	if (m_script) {
		player->setScript(m_script);
		m_script->onPlayerEvent("join", player);
	}

	if (peer_id == m_my_peer_id) {
		m_state = ClientState::WorldPlay;

		GameEvent e(GameEvent::C2G_JOIN);
		sendNewEvent(e);

		updateWorld(false);

		GameEvent e2(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e2);
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

	std::unique_ptr<LocalPlayer> player(getPlayerNoLock(peer_id));
	player->setScript(nullptr);
	m_players.erase(peer_id);

	if (player) {
		if (m_script)
			m_script->onPlayerEvent("leave", player.get());

		GameEvent e(GameEvent::C2G_PLAYER_LEAVE);
		e.player = player.get();
		sendNewEvent(e);
	}

	// HACK: keep alive until the function finishes !
	// gameplay.cpp might access the World object at the same time,
	// which somehow results in a double-free ??
	RefCnt<World> world;
	if (player)
		world = player->getWorld();

	if (peer_id == m_my_peer_id) {
		quitToLobby(player.release());
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

	if (reset_progress && my_player_affected) {
		// semi-duplicate of Player::updateCoinCount
		updateWorld(true);

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

void Client::pkt_ChatReplay(Packet &pkt)
{
	auto player = getMyPlayer();
	if (!player)
		return;

	auto &meta = player->getWorld()->getMeta();
	while (pkt.getRemainingBytes() > 0) {
		WorldMeta::ChatHistory entry;
		entry.timestamp = pkt.read<s64>();
		if (!entry.timestamp)
			break;

		entry.name = pkt.readStr16();
		entry.message = pkt.readStr16();
		meta.chat_history.push_back(std::move(entry));
	}

	GameEvent e(GameEvent::C2G_CHAT_HISTORY);
	sendNewEvent(e);
}

// Similar to Server::stepSendBlockUpdates
void Client::pkt_PlaceBlock(Packet &pkt)
{
	auto world = getWorld();
	if (!world)
		throw std::runtime_error("pkt_PlaceBlock: world not ready");

	auto player = getMyPlayer();
	SimpleLock world_lock(world->mutex);

	if (m_script)
		m_script->setPlayer(player.ptr());

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


	world_lock.unlock();

	player->updateCoinCount();

	GameEvent e(GameEvent::C2G_MAP_UPDATE);
	sendNewEvent(e);
}

void Client::pkt_ScriptEvent(Packet &pkt)
{
	if (!m_script || m_rl_scriptevents.isActive())
		return;
	if (pkt.data_version < 9)
		return; // incompatible

	auto world = getMyPlayer()->getWorld();

	// Similar to `Server::pkt_ScriptEvent` but with peer_id + Event
	auto *smgr = m_script->getSEMgr();
	ScriptEvent se;

	try {
		m_script->invoked_by_server = true;
		while (smgr->readNextEvent(pkt, true, se)) {
			if (m_rl_scriptevents.add(1))
				break; // discard. rate limit reached.

			if (se.first & SEF_HAVE_ACTOR) {
				Player *p = getPlayerNoLock(se.second.peer_id);
				if (!p)
					continue;

				m_script->setPlayer(p);
			} else {
				m_script->setWorld(world.get());
			}

			smgr->runLuaEventCallback(se);
		}

		m_script->invoked_by_server = false;
		m_script->setPlayer(nullptr);
	} catch (...) {
		m_script->invoked_by_server = false;
		m_script->setPlayer(nullptr);
		throw;
	}
}

void Client::pkt_ActivateBlock(Packet &pkt)
{
	/*
		In long term, key blocks (and others) should raise "events" from Lua-side.
	*/

	auto player = getMyPlayer();

	bid_t activated_id = pkt.read<bid_t>();
	bool state = pkt.read<u8>();

	bid_t bid_door,
		bid_gate,
		bid_aux = Block::ID_INVALID;

	switch (activated_id) {
		case Block::ID_KEY_R:
		case Block::ID_KEY_G:
		case Block::ID_KEY_B:
		{

			bid_t key_idx = activated_id - Block::ID_KEY_R;
			bid_door = key_idx + Block::ID_DOOR_R;
			bid_gate = key_idx + Block::ID_GATE_R;

			auto &timer = player->getWorld()->getMeta().keys[key_idx];
			timer.set(state); // 1.0f (active) or 0.0f (stopped)
		}
			break; // good, continue.
		case Block::ID_SWITCH:
		{
			u8 &val = player->getWorld()->getMeta().switch_state;
			if (val == state)
				return; // nothing to do

			val = state;
			bid_door = Block::ID_SWITCH_DOOR;
			bid_gate = Block::ID_SWITCH_GATE;
			bid_aux  = Block::ID_SWITCH;
		}
			break; // good, continue
		default:
			return; // unknown, unhandled block
	};

	// Quick iterate
	size_t n = 0;
	auto world = player->getWorld();
	for (Block *b = world->begin(); b < world->end(); ++b) {
		bid_t id = b->id;
		if (id == bid_door || id == bid_gate || id == bid_aux) {
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
	if (peer_id == m_my_peer_id && m_bmgr->isHardcoded()) {
		bool map_dirty = false;

		auto bump_tile = [state](Block &b) -> bool {
			if (state)
				b.tile++;
			else if (b.tile)
				b.tile--;
			else
				return false;
			return true;
		};

		auto out = player->getWorld()->getBlocks(Block::ID_SECRET, bump_tile);
		if (!out.empty())
			map_dirty = true;

		out = player->getWorld()->getBlocks(Block::ID_BLACKFAKE, bump_tile);
		if (!out.empty())
			map_dirty = true;

		if (map_dirty) {
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

		logger(LL_DEBUG, "pkt_PlayerFlags: player=%s, flags=%08X",
			player->name.c_str(), player->getFlags().flags
		);
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

void Client::pkt_Deprecated(Packet &pkt)
{
	logger(LL_WARN, "Ignoring deprecated packet %s", pkt.dump().c_str());
}
