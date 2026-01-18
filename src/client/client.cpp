#include "client.h"
#include "clientmedia.h"
#include "clientscript.h"
#include "localplayer.h"
#include "core/auth.h"
#include "core/blockmanager.h"
#include "core/connection.h"
#include "core/logger.h"
#include "core/network_enums.h"
#include "core/packet.h"
#include "core/utils.h" // to_player_name
#include "core/script/scriptevent.h"
#include "core/worldmeta.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

static Logger logger("Client", LL_DEBUG);

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor
extern BlockManager *g_blockmanager; // for client-only use

static const float POSITION_SEND_INTERVAL = 5.0f;

Client::Client(ClientStartData &init) :
	Environment(g_blockmanager),
	m_rl_scriptevents(1 / 20.0f, 2),
	m_start_data(std::move(init)) // eaten
{
	logger(LL_PRINT, "Startup ...");

	{
		PACKET_ACTIONS_MAX = 0;
		const ClientPacketHandler *handler = packet_actions;
		while (handler->func)
			handler++;

		PACKET_ACTIONS_MAX = handler - packet_actions;
		ASSERT_FORCED(PACKET_ACTIONS_MAX == (int)Packet2Client::MAX_END, "Packet handler mismatch");
	}

	m_pos_send_timer.set(POSITION_SEND_INTERVAL);
}

Client::~Client()
{
	logger(LL_PRINT, "Stopping ...");

	delete m_con;
	m_con = nullptr;

	{
		// In case a packet is being processed
		SimpleLock lock(m_players_lock);
		m_players.clear();
	}

	if (m_script) {
		delete m_script;
		m_script = nullptr;
	}

	if (m_bmgr == g_blockmanager) {
		// restore for reconnect
		delete g_blockmanager;
		g_blockmanager = new BlockManager();
	}

	if (m_media) {
		delete m_media;
		m_media = nullptr;
	}
}

// -------------- Public members -------------

void Client::prepareScript(ClientScript *script, bool need_audiovisuals)
{
	ASSERT_FORCED(!m_media && !m_script, "m_media or m_script double-init");

	// "Who calls who?" A little diagram.
	//
	//      .---- Script ----.
	//      v                v
	// BlockManager ---> ClientMedia
	//      ^
	//      |
	//     GUI

	m_media = new ClientMedia();
	m_media->download_audiovisuals = need_audiovisuals;
	if (need_audiovisuals)
		m_media->removeOldCache();
	m_media->indexAssets();

	m_bmgr->setMediaMgr(m_media);

	m_script = script ? script : new ClientScript(m_bmgr);
	m_script->setMediaMgr(m_media);
	m_script->setClient(this);
	ASSERT_FORCED(m_script->init(), "No future.");
}

void Client::connect()
{
	ASSERT_FORCED(!m_con, "m_con double-init");

	m_con = new Connection(Connection::TYPE_CLIENT, "Client");
	m_con->connect(m_start_data.address.c_str());
	m_con->listenAsync(*this);
}

void Client::step(float dtime)
{
	m_time_prev = m_time;
	m_time = getTimeNowDIV();

	switch (m_state) {
		case ClientState::MediaDownload:
			initScript();
			break;
		case ClientState::LobbyIdle:
			if (!m_start_data.world_id.empty()) {
				GameEvent e(GameEvent::G2C_JOIN);
				e.text = new std::string();
				std::swap(*e.text, m_start_data.world_id);
				OnEvent(e);
			}
		default:
			break;
	}

	auto world = getWorld();

	// Process block updates
	while (world.get()) { // run once
		SimpleLock lock(world->mutex);
		auto &queue = world->proc_queue;
		if (queue.empty())
			break;

		Packet out;
		out.write(Packet2Server::PlaceBlock);

		// Almost identical to the queue processing in Server::step()
		for (auto it = queue.cbegin(); it != queue.cend(); ++it) {

			out.write<u8>(true); // begin
			// Write BlockUpdate
			it->write(out);

			DEBUGLOG("Client: sending block x=%d,y=%d,id=%d\n",
				it->pos.X, it->pos.Y, it->getId());
		}
		out.write<u8>(false);
		queue.clear();

		m_con->send(0, 0, out);
		break;
	}

	// Run physics engine
	stepPhysics(dtime);

	if (world.get() && m_pos_send_timer.step(dtime)) {
		auto player = getMyPlayer();
		if (player->last_sent_pos != player->last_pos) {
			player.release();
			sendPlayerMove();
		}

		m_pos_send_timer.set(POSITION_SEND_INTERVAL);
	}

	if (m_script)
		m_script->onStep((double)m_time / TIME_RESOLUTION);

	// Timed gates update
	while (m_bmgr->isHardcoded() && world.get()) { // run once
		uint8_t tile_new = (m_time      / TIME_RESOLUTION) % 10;
		uint8_t tile_old = (m_time_prev / TIME_RESOLUTION) % 10;
		if (tile_new == tile_old)
			break;

		// TODO: Sync with the server
		PositionRange pr;
		pr.op = PositionRange::PROP_SET;
		pr.type = PositionRange::PRT_ENTIRE_WORLD;
		world->setBlockTiles(pr, Block::ID_TIMED_GATE_1, tile_new);
		world->setBlockTiles(pr, Block::ID_TIMED_GATE_2, tile_new);
		break;
	}

	// Update tiles if triggered by Lua
	if (world && world->modified_rect.isValid()) {
		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
	}
}

bool Client::OnEvent(GameEvent &e)
{
	using E = GameEvent::G2C_Enum;

	switch (e.type_g2c) {
		case E::G2C_INVALID:
			return false;
		case E::G2C_REGISTER:
			{
				m_auth.hash(m_auth.salt_1_const, *e.text);

				Packet pkt;
				pkt.write(Packet2Server::Auth);
				pkt.writeStr16("register");
				pkt.writeStr16(m_auth.output);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_LOBBY_REQUEST:
			{
				Packet pkt;
				pkt.write(Packet2Server::GetLobby);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_FRIEND_ACTION:
			{
				Packet pkt;
				pkt.write(Packet2Server::FriendAction);
				pkt.write<uint8_t>(e.friend_action->action);
				to_player_name(e.friend_action->player_name);
				pkt.writeStr16(e.friend_action->player_name);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_SET_PASSWORD:
			{
				m_auth.hash(m_auth.salt_1_const, e.password->old_pw);
				e.password->old_pw = m_auth.output;

				m_auth.hash(m_auth.salt_1_const, e.password->new_pw);
				e.password->new_pw = m_auth.output;

				Packet pkt;
				pkt.write(Packet2Server::Auth);
				pkt.writeStr16("setpass");
				pkt.writeStr16(e.password->old_pw);
				pkt.writeStr16(e.password->new_pw);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_JOIN:
			if (getWorld().get()) {
				// Already joined one. ignore.
				return false;
			}

			{
				m_state = ClientState::WorldJoin;
				m_world_id = *e.text;

				Packet pkt;
				pkt.write(Packet2Server::Join);
				pkt.writeStr16(m_world_id);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_CREATE_WORLD:
			if (getWorld().get()) {
				// Already joined one. ignore.
				return false;
			}

			{
				m_state = ClientState::WorldJoin;
				m_world_id.clear();

				Packet pkt;
				pkt.write(Packet2Server::Join);
				pkt.writeStr16("");
				pkt.write<u8>(e.wc_data->mode);
				blockpos_t size { 100, 100 };
				pkt.write(size.X);
				pkt.write(size.Y);
				pkt.writeStr16(e.wc_data->title);
				pkt.writeStr16(e.wc_data->code);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_LEAVE:
			if (!getWorld().get()) {
				// Already left the world
				return true;
			}

			{
				Packet pkt;
				pkt.write(Packet2Server::Leave);
				m_con->send(0, 0, pkt);
			}

			return true;
		case E::G2C_CHAT:
			if (e.text->empty())
				return false;

			{
				Packet pkt;
				pkt.write(Packet2Server::Chat);
				pkt.writeStr16(*e.text);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_GODMODE:
			{
				Packet pkt;
				pkt.write(Packet2Server::GodMode);
				pkt.write<u8>(e.intval);
				m_con->send(0, 1, pkt);
			}
			return true;
		case E::G2C_SMILEY:
			{
				Packet pkt;
				pkt.write(Packet2Server::Smiley);
				pkt.write<u8>(e.intval);
				m_con->send(0, 1, pkt);
			}
			return true;
		case E::G2C_GET_ASSET_PATH:
			if (!m_media)
				return false;

			e.asset_path.output = m_media->getAssetPath(e.asset_path.input);
			return true;
	}
	return false;
}

// -------------- Functions for the GUI -------------

std::string Client::getDebugInfo()
{
	std::string str;
	if (m_con)
		str = m_con->getDebugInfo(0);

	char buf[255];
	snprintf(buf, sizeof(buf),
		"Protocol version: cur=%i, max=%i\n"
		"Client state: %i\n"
		"Last world ID: %s\n",
		m_protocol_version, PROTOCOL_VERSION_MAX,
		(int)getState(),
		m_world_id.c_str()
	);

	str.append(buf);

	auto player = getMyPlayer();
	if (player.ptr() && player->getWorld()) {
		snprintf(buf, sizeof(buf),
			"Pos: (%.1f, %.1f)\n"
			"Vel: (%.1f, %.1f)\n"
			"Acc: (%.0f, %.0f)\n"
			"Events: %s%s%s\n",
			player->pos.X, player->pos.Y,
			player->vel.X, player->vel.Y,
			player->acc.X, player->acc.Y,
			(player->did_jerk ? "jerk " : ""),
			(player->controls_enabled ? "" : "locked "),
			(player->godmode ? "god " : "")
		);
		str.append(buf);
	}
	return str;
}


PtrLock<LocalPlayer> Client::getMyPlayer()
{
	m_players_lock.lock();
	return PtrLock<LocalPlayer>(m_players_lock, getPlayerNoLock(m_my_peer_id));
}

PtrLock<map_peer_player_t> Client::getPlayerList()
{
	m_players_lock.lock();
	return PtrLock<map_peer_player_t>(m_players_lock, &m_players);
}

RefCnt<World> Client::getWorld()
{
	auto player = getPlayerNoLock(m_my_peer_id);
	return player ? player->getWorld() : nullptr;
}


bool Client::updateBlock(BlockUpdate bu)
{
	auto world = getWorld();
	if (!world)
		return false;

	SimpleLock lock(world->mutex);

	if (!world->checkUpdateBlockNeeded(bu))
		return false;

	world->proc_queue.emplace(bu);
	return true;
}

void Client::clearTileCacheFor(bid_t block_id)
{
	m_tile_cache_mgr.clearCacheFor(block_id);
}


// -------------- Utility functions -------------

LocalPlayer *Client::getPlayerNoLock(peer_t peer_id)
{
	// It's not really a "peer" ID but player ID
	auto it = m_players.find(peer_id);
	return it != m_players.end()
		? (LocalPlayer *)it->second.get()
		: nullptr;
}

// -------------- Networking -------------

void Client::disconnect(const char *reason)
{
	GameEvent e(GameEvent::C2G_DIALOG);
	e.text = new std::string(reason ? reason : "Disconnect requested.");
	sendNewEvent(e);
	m_con->disconnect(Connection::PEER_ID_FIRST);
}

Packet Client::createPacket(Packet2Server type) const
{
	Packet pkt;
	pkt.data_version = m_protocol_version;
	pkt.write(type);
	return pkt;
}

void Client::sendPlayerMove()
{
	auto player = getMyPlayer();
	if (!player)
		return;

	Packet pkt = createPacket(Packet2Server::Move);
	player->step(0.0001f); // update controls
	player->writePhysics(pkt);
	m_con->send(0, 1 | Connection::FLAG_UNRELIABLE, pkt);

	player->last_sent_pos = player->last_pos;
	m_pos_send_timer.set(POSITION_SEND_INTERVAL);
}


void Client::onPeerConnected(peer_t)
{
	m_state = ClientState::Connected;
	logger(LL_PRINT, "Connected to server");

	ASSERT_FORCED(m_media, "Missing ClientMedia");

	Packet pkt;
	pkt.write(Packet2Server::Hello);
	pkt.write(PROTOCOL_VERSION_MAX);
	pkt.write(PROTOCOL_VERSION_MIN);
	pkt.writeStr16(m_start_data.nickname);
	pkt.write<u8>(m_media->download_audiovisuals);

	m_con->send(0, 0, pkt);
}

void Client::onPeerDisconnected(peer_t)
{
	m_state = ClientState::None;

	// send event for client destruction
	{
		GameEvent e2(GameEvent::C2G_DISCONNECT);
		sendNewEvent(e2);
	}
	logger(LL_PRINT, "Disconnected from the server");
}

void Client::processPacket(peer_t peer_id, Packet &pkt)
{
	// one server instance, multiple worlds
	int action = (int)pkt.read<Packet2Client>();
	if (action >= PACKET_ACTIONS_MAX) {
		logger(LL_ERROR, "Packet action %u out of range: %s", action, pkt.dump(20).c_str());
		return;
	}

	const ClientPacketHandler &handler = packet_actions[action];
	if ((int)handler.min_player_state > (int)m_state) {
		logger(LL_ERROR, "not ready for action=%d", action);
		return;
	}

	try {
		pkt.data_version = m_protocol_version;
		(this->*handler.func)(pkt);
	} catch (std::out_of_range &e) {
		Player *player = getPlayerNoLock(peer_id);
		logger(LL_ERROR, "Packet out_of_range. action=%d, player=%s, msg=%s",
			action,
			player ? player->name.c_str() : "(null)",
			e.what()
		);
	} catch (std::exception &e) {
		Player *player = getPlayerNoLock(peer_id);
		logger(LL_ERROR, "Packet exception. action=%d, player=%s, msg=%s",
			action,
			player ? player->name.c_str() : "(null)",
			e.what()
		);
	}
}

using blockdata_t = GameEvent::BlockData;
using blockdata_v_t = std::vector<blockdata_t>;

static blockdata_v_t send_on_touch_blocks(LocalPlayer *player, Packet &pkt)
{
	if (!player || !player->on_touch_blocks)
		throw std::runtime_error("null ptr");

	// Clear the list
	auto on_touch_blocks = std::move(player->on_touch_blocks);
	blockdata_v_t gameevents;
	gameevents.reserve(on_touch_blocks->size());

	bool needs_coins_update = false;

	pkt.write(Packet2Server::OnTouchBlock);

	const auto world = player->getWorld().get();
	auto &meta = world->getMeta();

	for (blockpos_t bp : *on_touch_blocks) {
		Block b;
		if (!world->getBlock(bp, &b))
			continue;

		switch (b.id) {
			case Block::ID_KEY_R:
			case Block::ID_KEY_G:
			case Block::ID_KEY_B:
			{
				int key_id = b.id - Block::ID_KEY_R;
				auto &kdata = meta.keys[key_id];
				// Use timer client-sided to avoid sending duplicates too often
				if (kdata.set(-1.0f)) {
					pkt.write(bp.X);
					pkt.write(bp.Y);
				}
			}
			break;
			case Block::ID_COIN:
			case Block::ID_SECRET:
			case Block::ID_BLACKFAKE:
				if (b.tile)
					break;

				b.tile = 1;
				world->setBlock(bp, b);

				if (b.id == Block::ID_COIN) {
					needs_coins_update = true;
					gameevents.emplace_back(blockdata_t {bp, b});
				}
			break;
			case Block::ID_PIANO:
				gameevents.emplace_back(blockdata_t {bp, b});
				break;
			case Block::ID_SWITCH:
				pkt.write(bp.X);
				pkt.write(bp.Y);
			break;
			case Block::ID_CHECKPOINT:
				if (player->godmode)
					break;
				{
					// Unmark previous checkpoint
					Block b2;
					world->getBlock(player->checkpoint, &b2);
					if (b2.id == Block::ID_CHECKPOINT) {
						b2.tile = 0;
						world->setBlock(player->checkpoint, b2);
					}
				}
				{
					// Mark current checkpoint
					player->checkpoint = bp;
					b.tile = 1;
					world->setBlock(player->checkpoint, b);
				}
				// fall-through
			case Block::ID_SPIKES:
				if (player->godmode)
					break;
				{
					pkt.write(bp.X);
					pkt.write(bp.Y);
				}
			break;
		}
	} // for

	if (needs_coins_update)
		player->updateCoinCount(false);

	return gameevents;
}

void Client::stepPhysics(float dtime)
{
	auto world = getWorld();
	if (!world.get())
		return;

	SimpleLock lock(m_players_lock);

	auto player = getPlayerNoLock(m_my_peer_id);
	player->on_touch_blocks.reset(new std::set<blockpos_t>());

	FOR_PLAYERS(, player, m_players) {
		player->step(dtime);
	}

	// Process touch events. Should be used for hardcoded only.
	blockdata_v_t gameevents;
	{
		Packet pkt;
		gameevents = send_on_touch_blocks(player, pkt);

		if (pkt.size() > 2) {
			pkt.write(BLOCKPOS_INVALID); // terminating position

			m_con->send(0, 1, pkt);
		}
	}
	for (const blockdata_t bd : gameevents) {
		GameEvent e(GameEvent::C2G_ON_TOUCH_BLOCK);
		e.block = new blockdata_t(bd);
		sendNewEvent(e);
	}

	ScriptEventMap *se = player->script_events_to_send.get();
	if (se && !se->empty()) {
		// Read back by `Server::pkt_ScriptEvent`
		Packet pkt;
		pkt.write(Packet2Server::ScriptEvent);
		size_t count = m_script->getSEMgr()->writeBatchNT(pkt, false, se);
		pkt.write<u16>(UINT16_MAX); // end of batch
		if (count > 0)
			m_con->send(0, 1, pkt);
		se->clear();
	}
	m_rl_scriptevents.step(dtime);
}

void Client::initScript()
{
	ASSERT_FORCED(m_media && m_script, "Missing media or script");

	if (m_media->countDone() == 0 || m_media->countMissing() > 0)
		return; // not yet

	if (!m_script->loadFromAsset("main.lua"))
		goto error;

	m_script->onScriptsLoaded();
	m_script->setMyPlayer(getPlayerNoLock(m_my_peer_id));
	m_state = ClientState::LobbyIdle;
	// maybe generate an event?

	return; // OK

error:
	logger(LL_ERROR, "Failed to initialize script. Falling back to hardcoded.");
	m_bmgr->doPackRegistration();
	m_state = ClientState::LobbyIdle;
}


uint8_t Client::getBlockTile(const Player *player, const Block *b)
{
	if (!m_bmgr->isHardcoded()) {
		return m_tile_cache_mgr.getOrCache(b).tile;
	}

	auto world = player->getWorld();

	auto get_params = [&world, b] () {
		BlockParams params;
		world->getParams(world->getBlockPos(b), &params);
		return params;
	};

	switch (b->id) {
		case Block::ID_SPIKES:
			return get_params().param_u8;
		case Block::ID_SECRET:
		case Block::ID_BLACKFAKE:
			if (uint8_t tile = b->tile)
				return tile;
			return player->godmode;
		case Block::ID_TELEPORTER:
			return get_params().teleporter.rotation;
		case Block::ID_COIN:
			return b->tile;
		case Block::ID_COINDOOR:
		case Block::ID_COINGATE:
			return player->coins >= get_params().param_u8;
		case Block::ID_DOOR_R:
		case Block::ID_DOOR_G:
		case Block::ID_DOOR_B:
			return world->getMeta().keys[b->id - Block::ID_DOOR_R].isActive();
		case Block::ID_GATE_R:
		case Block::ID_GATE_G:
		case Block::ID_GATE_B:
			return world->getMeta().keys[b->id - Block::ID_GATE_R].isActive();
		case Block::ID_SWITCH:
		case Block::ID_SWITCH_DOOR:
		case Block::ID_SWITCH_GATE:
			return world->getMeta().switch_state;
		case Block::ID_TIMED_GATE_1:
		case Block::ID_TIMED_GATE_2:
			return (m_time / TIME_RESOLUTION) % 10;
	}
	return 0;
}

void Client::updateAllBlockTiles(bool reset_tiles)
{
	const bool is_hardcoded = m_bmgr->isHardcoded();

	auto player = getPlayerNoLock(m_my_peer_id);
	auto world = player->getWorld();

	SimpleLock lock(world->mutex);

	if (m_script)
		m_script->setPlayer(player);

	if (reset_tiles)
		m_tile_cache_mgr.clearAll();

	if (!is_hardcoded && m_tile_cache_mgr.removed_caches_counter == 0)
		return; // nothing to update

	m_tile_cache_mgr.cache_miss_counter = 0;
	m_tile_cache_mgr.removed_caches_counter = 0;

	// Restore visuals to Block::tile after new world data
	for (Block *b = world->begin(); b < world->end(); ++b) {
		if (reset_tiles)
			b->tile = 0;
		b->tile = getBlockTile(player, b);
	}
	if (is_hardcoded || m_tile_cache_mgr.cache_miss_counter > 0) {
		world->markAllModified();
	}
}

void Client::quitToLobby(Player *p_to_keep)
{
	const peer_t peer_ignored = p_to_keep->peer_id;
	for (auto &p : m_players) {
		if (p.second.get() == p_to_keep) {
			p.second.release(); // do NOT free myself
		}
	}
	m_players.clear();

	// Keep myself in the list
	if (p_to_keep) {
		m_players.emplace(peer_ignored, p_to_keep);
		p_to_keep->setWorld(nullptr);
	}

	m_tile_cache_mgr.reset();

	m_state = ClientState::LobbyIdle;

	GameEvent e(GameEvent::C2G_LEAVE);
	sendNewEvent(e);
}

