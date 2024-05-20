#include "client.h"
#include "clientmedia.h"
#include "localplayer.h"
#include "core/auth.h"
#include "core/blockmanager.h"
#include "core/connection.h"
#include "core/logger.h"
#include "core/packet.h"
#include "core/script.h"
#include "core/utils.h" // to_player_name

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

	{
		// In case a packet is being processed
		SimpleLock lock(m_players_lock);

		for (auto it : m_players)
			delete it.second;
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
	delete m_con;
}

// -------------- Public members -------------

void Client::setupMedia(bool need_audiovisuals)
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

	m_script = new Script(m_bmgr);
	m_script->setMediaMgr(m_media);
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

	if (m_state == ClientState::MediaDownload) {
		initScript();
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

	// Timed gates update
	while (world.get()) { // run once
		uint8_t tile_new = (m_time      / TIME_RESOLUTION) % 10;
		uint8_t tile_old = (m_time_prev / TIME_RESOLUTION) % 10;
		if (tile_new == tile_old)
			break;

		// TODO: Sync with the server
		bool updated = false;
		for (Block *b = world->begin(); b < world->end(); ++b) {
			if (b->id == Block::ID_TIMED_GATE_1
					|| b->id == Block::ID_TIMED_GATE_2) {
				b->tile = tile_new;
				updated = true;
			}
		}

		if (updated) {
			GameEvent e(GameEvent::C2G_MAP_UPDATE);
			sendNewEvent(e);
		}
		break;
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
				blockpos_t size { 200, 200 };
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
			break;
		case E::G2C_SMILEY:
			{
				Packet pkt;
				pkt.write(Packet2Server::Smiley);
				pkt.write<u8>(e.intval);
				m_con->send(0, 1, pkt);
			}
			break;
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

PtrLock<decltype(Client::m_players)> Client::getPlayerList()
{
	m_players_lock.lock();
	return PtrLock<decltype(m_players)>(m_players_lock, &m_players);
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

// -------------- Utility functions -------------

LocalPlayer *Client::getPlayerNoLock(peer_t peer_id)
{
	// It's not really a "peer" ID but player ID
	auto it = m_players.find(peer_id);
	return it != m_players.end() ? dynamic_cast<LocalPlayer *>(it->second) : nullptr;
}

// -------------- Networking -------------

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
	pkt.write<u8>(m_media->download_audiovisuals); // proto_ver >= 7

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

void Client::stepPhysics(float dtime)
{
	auto world = getWorld();
	if (!world.get())
		return;

	std::set<blockpos_t> on_touch_blocks;
	SimpleLock lock(m_players_lock);

	for (auto it : m_players) {
		if (it.first == m_my_peer_id) {
			it.second->on_touch_blocks = &on_touch_blocks;
			it.second->step(dtime);
			it.second->on_touch_blocks = nullptr;
		} else {
			it.second->step(dtime);
		}
	}

	auto &meta = world->getMeta();

	// Process node events
	bool map_dirty = false;
	Packet pkt;
	pkt.write(Packet2Server::OnTouchBlock);

	auto player = getPlayerNoLock(m_my_peer_id);
	for (blockpos_t bp : on_touch_blocks) {
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
				map_dirty = true;

				if (b.id == Block::ID_COIN) {
					GameEvent e(GameEvent::C2G_ON_TOUCH_BLOCK);
					e.block = new GameEvent::BlockData();
					e.block->b = b;
					e.block->pos = bp;
					sendNewEvent(e);
				}
			break;
			case Block::ID_PIANO:
				{
					GameEvent e(GameEvent::C2G_ON_TOUCH_BLOCK);
					e.block = new GameEvent::BlockData();
					e.block->b = b;
					e.block->pos = bp;
					sendNewEvent(e);
				}
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
					map_dirty = true;
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

	if (pkt.size() > 4) {
		pkt.write(BLOCKPOS_INVALID); // end

		m_con->send(0, 1, pkt);
	}

	if (map_dirty) {
		// Triggering an event anyway. Update coin doors.
		auto player = getPlayerNoLock(m_my_peer_id);
		player->updateCoinCount();

		GameEvent e(GameEvent::C2G_MAP_UPDATE);
		sendNewEvent(e);
	}
}

void Client::initScript()
{
	ASSERT_FORCED(m_media && m_script, "Missing media or script");

	if (m_media->countDone() == 0 || m_media->countMissing() > 0)
		return; // not yet

	if (!m_script->loadFromAsset("main.lua"))
		goto error;

	m_state = ClientState::LobbyIdle;
	// maybe generate an event?

	return; // OK

error:
	logger(LL_ERROR, "Failed to initialize script. Falling back to hardcoded.");
	m_bmgr->doPackRegistration();
	m_state = ClientState::LobbyIdle;
}


uint8_t Client::getBlockTile(const Player *player, const Block *b) const
{
	auto world = player->getWorld();

	auto get_params = [&] () {
		BlockParams params;
		world->getParams(world->getBlockPos(b), &params);
		return params;
	};

	switch (b->id) {
		case Block::ID_SPIKES:
			return get_params().param_u8;
		case Block::ID_SECRET:
		case Block::ID_BLACKFAKE:
			return player->godmode;
		case Block::ID_TELEPORTER:
			return get_params().teleporter.rotation;
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
		case Block::ID_TIMED_GATE_1:
		case Block::ID_TIMED_GATE_2:
			return (m_time / TIME_RESOLUTION) % 10;
	}
	return 0;
}

void Client::updateWorld()
{
	auto player = getPlayerNoLock(m_my_peer_id);
	auto world = player->getWorld();

	// Restore visuals to Block::tile after new world data
	for (Block *b = world->begin(); b < world->end(); ++b) {
		b->tile = getBlockTile(player, b);
	}
}

