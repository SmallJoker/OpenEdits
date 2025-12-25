#include "server.h"
#include "database_auth.h"
#include "database_world.h"
#include "remoteplayer.h"
#include "servermedia.h"
#include "serverscript.h"
#include "core/blockmanager.h"
#include "core/logger.h"
#include "core/network_enums.h"
#include "core/packet.h"
#include "core/world.h"
#include "core/worldmeta.h"
#include "core/script/scriptevent.h"
#include "version.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

static Logger logger("Server", LL_WARN);

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Server::Server(bool *shutdown_requested) :
	Environment(new BlockManager()),
	m_shutdown_requested(shutdown_requested)
{
	logger(LL_PRINT, "Startup ...");
	m_stdout_flush_timer.set(1);
	m_ban_cleanup_timer.set(2);

	m_con = new Connection(Connection::TYPE_SERVER, "Server");
	if (!m_con->listenAsync(*this)) {
		goto error;
	}

	{
		// Initialize persistent world storage
		m_world_db = new DatabaseWorld();
		if (!m_world_db->tryOpen("server_worlddata.sqlite")) {
			logger(LL_ERROR, "Failed to open world database!");
			delete m_world_db;
			m_world_db = nullptr;
			goto error;
		}
	}

	{
		// Initialize auth
		m_auth_db = new DatabaseAuth();
		if (m_auth_db->tryOpen("server_auth.sqlite")) {
			// allow access from CLI
			m_auth_db->enableWAL();
		} else {
			logger(LL_ERROR, "Failed to open auth database!");
			delete m_auth_db;
			m_auth_db = nullptr;
			goto error;
		}
	}

	m_script = new ServerScript(m_bmgr, this);
	if (!m_script->init()) {
		logger(LL_ERROR, "Failed to initialize Lua");
		goto error;
	}

	m_media = new ServerMedia();


	// Initialize Script + Assets needed for the clients
	{
		m_media->indexAssets();
		m_script->setMediaMgr(m_media);

		if (!m_script->loadFromAsset("main.lua")) {
			logger(LL_ERROR, "No future without main script");
			goto error;
		}

		m_bmgr->setMediaMgr(m_media);
		m_script->onScriptsLoaded();
		m_bmgr->sanityCheck();
		m_media_unload_timer.set(4);
	}

	registerChatCommands();

	{
		PACKET_ACTIONS_MAX = 0;
		const ServerPacketHandler *handler = packet_actions;
		while (handler->func)
			handler++;

		PACKET_ACTIONS_MAX = handler - packet_actions;
		ASSERT_FORCED(PACKET_ACTIONS_MAX == (int)Packet2Server::MAX_END, "Packet handler mismatch");
	}

	return; // OK
error:
	shutdown();
}

Server::~Server()
{
	logger(LL_PRINT, "Stopping ...");

	{
		// In case a packet is being processed
		SimpleLock lock(m_players_lock);
		m_players.clear();
	}

	if (m_media) {
		delete m_media;
		m_media = nullptr;
	}

	delete m_con;
	delete m_auth_db;
	delete m_world_db;
	delete m_bmgr;
}


// -------------- Public members -------------

void Server::step(float dtime)
{
	if (m_is_first_step) {
		logger(LL_PRINT, "Up and runnning.");
		m_is_first_step = false;
	}

	// always player lock first, world lock after.
	SimpleLock players_lock(m_players_lock);
	std::set<RefCnt<World>> worlds;
	for (auto &p : m_players) {
		RemotePlayer *player = (RemotePlayer *)p.second.get();
		auto world = player->getWorld();
		if (world) {
			player->rl_blocks.step(dtime);
			player->rl_chat.step(dtime);
			player->rl_scriptevents.step(dtime);
			player->time_since_move_pkt += dtime;

			worlds.emplace(world);
		}

		stepSendMedia(player);
	}

	// Process script events
	for (auto &p : m_players) {
		RemotePlayer *player = (RemotePlayer *)p.second.get();
		auto world = player->getWorld();

		if (!world)
			continue;

		// Clear the Player data
		std::unique_ptr<ScriptEventMap> player_se = std::move(player->script_events_to_send);

		if (player->protocol_version < 9)
			continue; // packet ID used differently. logs a warning.

		const ScriptEventMap *world_se = world->getMeta().script_events_to_send.get();
		if (!player_se && !world_se)
			continue; // nothing to send

		// See also: Client::stepPhysics
		Packet pkt;
		pkt.write(Packet2Client::ScriptEvent);
		size_t count = 0;
		count += m_script->getSEMgr()->writeBatchNT(pkt, true, player_se.get());
		count += m_script->getSEMgr()->writeBatchNT(pkt, true, world_se);
		pkt.write<u16>(UINT16_MAX); // terminate batch
		if (count > 0)
			m_con->send(p.first, 1, pkt);
	}

	// Clear world-specific event send queue
	for (auto &world : worlds)
		world->getMeta().script_events_to_send.release();

	if (m_script)
		m_script->onStep((double)getTimeNowDIV() / TIME_RESOLUTION);

	for (auto &world : worlds) {
		stepSendBlockUpdates(world.get());
		stepWorldTick(world.get(), dtime);
	}

	auto respawn_killed = [this] (Player *player) {
		Block b;
		auto world = player->getWorld();
		if (!world)
			return;

		world->getBlock(player->checkpoint, &b);
		if (b.id == Block::ID_CHECKPOINT) {
			teleportPlayer(player, core::vector2df(player->checkpoint.X, player->checkpoint.Y), false);
		} else {
			player->checkpoint = blockpos_t(-1, -1);
			respawnPlayer(player, true, false);
		}
	};

	// Respawn dead players
	for (auto it = m_deaths.begin(); it != m_deaths.end(); ) {
		if (!it->second.step(dtime)) {
			// Waiting
			it++;
			continue;
		}

		Player *player = getPlayerNoLock(it->first);
		if (player && !player->godmode)
			respawn_killed(player);

		it = m_deaths.erase(it);
	}

	// TODO: Run player physics to check whether they are cheating or not

	if (m_media_unload_timer.step(dtime)) {
		m_media_unload_timer.set(30);

		m_media->uncacheMedia();
	}

	if (m_stdout_flush_timer.step(dtime)) {
		/*
			When stdout and stderr are redirected to files, manual flushing
			becomes necessary to view the logs, e.g. for server overview.
		*/
		m_stdout_flush_timer.set(7);

		fflush(stdout);
		fflush(stderr);
	}

	if (m_ban_cleanup_timer.step(dtime)) {
		m_ban_cleanup_timer.set(65);

		if (m_auth_db)
			m_auth_db->cleanupBans();
	}

	if (m_shutdown_timer_remind.step(dtime)) {
		float rem = m_shutdown_timer.remainder();
		char tbuf[50];
		if (rem > 2.0f * 60.0f)
			snprintf(tbuf, sizeof(tbuf), "%d minute(s)", (int)rem / 60);
		else
			snprintf(tbuf, sizeof(tbuf), "%d second(s)", (int)rem);

		char mbuf[200];
		snprintf(mbuf, sizeof(mbuf), "Server will shut down in %s. Save your changes!", tbuf);

		// Send to all connected players
		for (auto &p : m_players) {
			sendMsg(p.first, mbuf);
			if (p.second->getWorld())
				systemChatSend(p.second.get(), mbuf);
		}

		// Reload countdown timer if needed
		if (rem > 6.0f * 60.0f)
			m_shutdown_timer_remind.set(5.0f * 60.0f);
		else if (rem > 2.0f * 60.0f)
			m_shutdown_timer_remind.set(0.5f * 60.0f);
		else
			m_shutdown_timer_remind.set(10.0f);

		// For debugging only
		if (false && m_shutdown_timer_remind.isActive()) {
			m_shutdown_timer.set(rem - m_shutdown_timer_remind.remainder() + 0.5f);
			m_shutdown_timer_remind.set(0.5f);
		}
	}

	if (m_shutdown_timer.step(dtime)) {
		if (m_shutdown_requested)
			*m_shutdown_requested = true;
	}

	m_static_lobby_worlds_timer.step(dtime);
}

// -------------- Utility functions --------------

RemotePlayer *Server::getPlayerNoLock(peer_t peer_id) const
{
	auto it = m_players.find(peer_id);
	return it != m_players.end()
		? (RemotePlayer *)it->second.get()
		: nullptr;
}

RefCnt<World> Server::getWorldNoLock(const std::string &id) const
{
	for (auto &p : m_players) {
		const auto world = p.second->getWorld();
		if (!world)
			continue;

		if (world->getMeta().id == id)
			return world;
	}
	return nullptr;
}

std::vector<Player *> Server::getPlayersNoLock(const World *world) const
{
	std::vector<Player *> ret;
	for (auto &p : m_players) {
		RemotePlayer *rp = (RemotePlayer *)p.second.get();
		if (rp->getWorld().get() != world)
			continue;

		if (rp->state != RemotePlayerState::WorldPlay)
			continue;

		ret.push_back(rp);
	}
	return ret;
}

// -------------- Networking --------------

void Server::onPeerConnected(peer_t peer_id)
{
	if (0) {
		Packet pkt;
		pkt.write<Packet2Client>(Packet2Client::Quack);
		pkt.writeStr16("hello world");
		m_con->send(peer_id, 0, pkt);
	}
}

void Server::onPeerDisconnected(peer_t peer_id)
{
	SimpleLock lock(m_players_lock);

	std::unique_ptr<Player> player;
	{
		auto it = m_players.find(peer_id);
		if (it == m_players.end())
			return; // not found
		player = std::move(it->second);
	}
	assert(player);
	m_players.erase(peer_id);

	logger(LL_DEBUG, "Player %s disconnected\n", player->name.c_str());
	sendPlayerLeave((RemotePlayer *)player.get());

	if (m_script)
		m_script->removePlayer(player.get());
}

void Server::processPacket(peer_t peer_id, Packet &pkt)
{
	// one server instance, multiple worlds
	int action = (int)pkt.read<Packet2Server>();
	if (action >= PACKET_ACTIONS_MAX) {
		logger(LL_ERROR, "Packet action %u out of range (peer_id=%d)\n", action, peer_id);
		return;
	}

	const ServerPacketHandler &handler = packet_actions[action];

	SimpleLock lock(m_players_lock);

	if (handler.min_player_state != RemotePlayerState::Invalid) {
		RemotePlayer *player = getPlayerNoLock(peer_id);
		if (!player) {
			logger(LL_ERROR, "Player peer_id=%u not found.\n", peer_id);
			return;
		}
		if ((int)handler.min_player_state > (int)player->state) {
			logger(LL_ERROR, "peer_id=%u is not ready for action=%d.\n", peer_id, action);
			return;
		}

		pkt.data_version = player->protocol_version;
	}

	try {
		(this->*handler.func)(peer_id, pkt);
	} catch (std::out_of_range &e) {
		Player *player = getPlayerNoLock(peer_id);
		logger(LL_ERROR, "Packet out_of_range. action=%d, player=%s, msg=%s\n",
			action,
			player ? player->name.c_str() : "(null)",
			e.what()
		);
	} catch (std::exception &e) {
		Player *player = getPlayerNoLock(peer_id);
		logger(LL_ERROR, "Packet exception. action=%d, player=%s, msg=%s\n",
			action,
			player ? player->name.c_str() : "(null)",
			e.what()
		);
	}
}

void Server::stepSendMedia(RemotePlayer *player)
{
	ASSERT_FORCED(m_media, "Missing ServerMedia");

	if (player->media.requested.empty())
		return;
	if (m_con->getPeerBytesInTransit(player->peer_id) > 5 * CONNECTION_MTU)
		return; // wait a bit

	Packet out = player->createPacket(Packet2Client::MediaReceive);
	m_media->writeMediaData(player, out);
	m_con->send(player->peer_id, 0, out);
}

// Similar to Client::pkt_PlaceBlock
void Server::stepSendBlockUpdates(World *world)
{
	// Process block placement queue of this world for broacast
	auto &queue = world->proc_queue;
	if (queue.empty())
		return;

	Packet out;
	out.write(Packet2Client::PlaceBlock);

	SimpleLock world_lock(world->mutex);
	auto it = queue.cbegin();
	for (; it != queue.cend(); ++it) {
		// Fit everything into an MTU
		if (out.size() > CONNECTION_MTU)
			break;

		// Distribute valid changes to players

		out.write(it->peer_id); // continue
		// Write BlockUpdate
		it->write(out);

		DEBUGLOG("Server: sending block x=%d,y=%d,id=%d\n",
			it->pos.X, it->pos.Y, it->getId());
	}

	queue.erase(queue.cbegin(), it); // "it" is not removed.
	world_lock.unlock();

	broadcastInWorld(world, 0, out);
}

void Server::stepWorldTick(World *world, float dtime)
{
	auto &meta = world->getMeta();

	for (auto &kdata : meta.keys) {
		if (kdata.step(dtime)) {
			// Disable keys

			kdata.stop();
			bid_t block_id = (&kdata - meta.keys) + Block::ID_KEY_R;
			Packet out;
			out.write(Packet2Client::ActivateBlock);
			out.write(block_id);
			out.write<u8>(false);

			broadcastInWorld(world, 0, out);
		}
	}

	// Compare new value vs old value
	bool sw_old = meta.switch_state & 0x01;
	bool sw_new = meta.switch_state & 0x80;
	if (sw_new != sw_old) {
		Packet out;
		out.write(Packet2Client::ActivateBlock);
		out.write<bid_t>(Block::ID_SWITCH);
		out.write<u8>(sw_new);
		broadcastInWorld(world, 1, out);
	}
	meta.switch_state = sw_new * 0x81;
}

bool Server::loadWorldNoLock(World *world)
{
	return m_world_db && world && m_world_db->load(world);
}

void Server::writeWorldData(Packet &out, World &world, bool is_clear)
{
	out.write(Packet2Client::WorldData);
	out.write<u8>(1 + is_clear); // 1: new data. 2: clear

	world.getMeta().writeCommon(out);
	world.getMeta().writeSpecific(out);
	blockpos_t size = world.getSize();
	out.write(size.X); // dimensions
	out.write(size.Y);
	if (!is_clear) {
		world.write(out, World::Method::Plain);
	}
}

void Server::setDefaultPlayerFlags(Player *player)
{
	auto world = player->getWorld();
	if (!world)
		return;

	WorldMeta &meta = world->getMeta();
	PlayerFlags pf = meta.getPlayerFlags(player->name);
	// Owner permssions
	if (meta.owner == player->name) {
		pf.set(PlayerFlags::PF_OWNER, PlayerFlags::PF_MASK_WORLD);
	}

	// Grant admin permissions if applicable
	if (m_auth_db) {
		AuthAccount auth;
		m_auth_db->load(player->name, &auth);
		switch (auth.level) {
			case AuthAccount::AL_MODERATOR:
				pf.set(PlayerFlags::PF_MODERATOR, PlayerFlags::PF_MASK_SERVER);
				break;
			case AuthAccount::AL_SERVER_ADMIN:
				pf.set(PlayerFlags::PF_ADMIN, PlayerFlags::PF_MASK_SERVER);
				break;
			default: break;
		}
	}

	DEBUGLOG("setDefaultPlayerFlags: name=%s, flags=%08X\n", player->name.c_str(), pf.flags);
	meta.setPlayerFlags(player->name, pf);

	DEBUGLOG("\t-> readback=%08X\n", meta.getPlayerFlags(player->name).flags);
}

void Server::teleportPlayer(Player *player, core::vector2df dst, bool reset_progress)
{
	Packet pkt;
	pkt.write(Packet2Client::SetPosition);
	pkt.write<u8>(reset_progress); // reset progress
	pkt.write(player->peer_id);
	pkt.write(dst.X);
	pkt.write(dst.Y);

	// Same channel as world data
	broadcastInWorld(player, 0, pkt);

	player->setPosition(dst, reset_progress);
}

void Server::respawnPlayer(Player *player, bool send_packet, bool reset_progress)
{
	if (player->godmode)
		return;

	auto &meta = player->getWorld()->getMeta();
	auto blocks = player->getWorld()->getBlocks(Block::ID_SPAWN, nullptr);

	if (blocks.empty()) {
		player->pos = core::vector2df();
	} else {
		int index = meta.spawn_index;
		if (++index >= (int)blocks.size())
			index = 0;

		player->pos.X = blocks[index].X;
		player->pos.Y = blocks[index].Y;
		meta.spawn_index = index;
	}

	if (send_packet)
		teleportPlayer(player, player->pos, reset_progress);
	else
		player->setPosition(player->pos, reset_progress);
}

void Server::shutdown()
{
	ASSERT_FORCED(m_shutdown_requested, "Cannot request shutdown.");
	*m_shutdown_requested = true;
}


