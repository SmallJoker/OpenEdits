#include "server.h"
#include "remoteplayer.h"
#include "core/blockmanager.h"
#include "core/packet.h"
#include "core/world.h"
#include "server/database_auth.h"
#include "server/database_world.h"
#include "version.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Server::Server(bool *shutdown_requested) :
	Environment(new BlockManager())
{
	puts("Server: startup");
	m_stdout_flush_timer.set(1);
	m_ban_cleanup_timer.set(2);

	m_bmgr->doPackRegistration();

	m_con = new Connection(Connection::TYPE_SERVER, "Server");
	if (!m_con->listenAsync(*this)) {
		if (shutdown_requested)
			*shutdown_requested = true;
		return;
	}

	{
		// Initialize persistent world storage
		m_world_db = new DatabaseWorld();
		if (!m_world_db->tryOpen("server_worlddata.sqlite")) {
			fprintf(stderr, "Failed to open world database!\n");
			delete m_world_db;
			m_world_db = nullptr;
			if (shutdown_requested)
				*shutdown_requested = true;
		}
	}

	{
		// Initialize auth
		m_auth_db = new DatabaseAuth();
		if (m_auth_db->tryOpen("server_auth.sqlite")) {
			m_auth_db->enableWAL();
		} else {
			fprintf(stderr, "Failed to open auth database!\n");
			delete m_auth_db;
			m_auth_db = nullptr;
			if (shutdown_requested)
				*shutdown_requested = true;
		}
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
}

Server::~Server()
{
	puts("Server: stopping...");

	{
		// In case a packet is being processed
		SimpleLock lock(m_players_lock);

		for (auto it : m_players)
			delete it.second;
		m_players.clear();
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
		puts("Server: Up and runnning.");
		m_is_first_step = false;
	}

	// maybe run player physics?

	// always player lock first, world lock after.
	SimpleLock players_lock(m_players_lock);
	std::set<RefCnt<World>> worlds;
	for (auto p : m_players) {
		auto world = p.second->getWorld();
		if (!world)
			continue;

		RemotePlayer *player = (RemotePlayer *)p.second;
		player->rl_blocks.step(dtime);
		player->rl_chat.step(dtime);

		worlds.emplace(world);

		// Process block placement queue of this world for broacast
		auto &queue = world->proc_queue;
		if (queue.empty())
			continue;

		SimpleLock world_lock(world->mutex);

		Packet out;
		out.write(Packet2Client::PlaceBlock);

		for (auto it = queue.cbegin(); it != queue.cend();) {
			// Distribute valid changes to players

			out.write(it->peer_id); // continue
			// Write BlockUpdate
			it->write(out);

			DEBUGLOG("Server: sending block x=%d,y=%d,id=%d\n",
				it->pos.X, it->pos.Y, it->getId());

			it = queue.erase(it);

			// Fit everything into an MTU
			if (out.size() > CONNECTION_MTU)
				break;
		}
		broadcastInWorld(world.get(), 0, out);

	}

	for (auto &world : worlds) {
		auto &meta = world->getMeta();

		for (auto &kdata : meta.keys) {
			if (kdata.step(dtime)) {
				// Disable keys

				kdata.stop();
				bid_t block_id = (&kdata - meta.keys) + Block::ID_KEY_R;
				Packet out;
				out.write(Packet2Client::Key);
				out.write(block_id);
				out.write<u8>(false);

				for (auto it : m_players) {
					if (it.second->getWorld() != world)
						continue;

					m_con->send(it.first, 0, out);
				}
			}
		}
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

	m_importable_worlds_timer.step(dtime);
}

// -------------- Utility functions --------------

RemotePlayer *Server::getPlayerNoLock(peer_t peer_id)
{
	auto it = m_players.find(peer_id);
	return it != m_players.end() ? dynamic_cast<RemotePlayer *>(it->second) : nullptr;
}

RefCnt<World> Server::getWorldNoLock(std::string &id)
{
	for (auto p : m_players) {
		auto world = p.second->getWorld();
		if (!world)
			continue;

		if (world->getMeta().id == id)
			return world;
	}
	return nullptr;
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

	auto player = getPlayerNoLock(peer_id);
	if (!player)
		return;

	printf("Server: Player %s disconnected\n", player->name.c_str());

	{
		Packet pkt;
		pkt.write(Packet2Client::Leave);
		pkt.write(peer_id);
		broadcastInWorld(player, 0, pkt);
	}

	player->setWorld(nullptr);
	m_players.erase(peer_id);

	delete player;
}

void Server::processPacket(peer_t peer_id, Packet &pkt)
{
	// one server instance, multiple worlds
	int action = (int)pkt.read<Packet2Server>();
	if (action >= PACKET_ACTIONS_MAX) {
		printf("Server: Packet action %u out of range\n", action);
		return;
	}

	const ServerPacketHandler &handler = packet_actions[action];

	SimpleLock lock(m_players_lock);

	if (handler.min_player_state != RemotePlayerState::Invalid) {
		RemotePlayer *player = getPlayerNoLock(peer_id);
		if (!player) {
			printf("Server: Player peer_id=%u not found.\n", peer_id);
			return;
		}
		if ((int)handler.min_player_state > (int)player->state) {
			printf("Server: peer_id=%u is not ready for action=%d.\n", peer_id, action);
			return;
		}

		pkt.data_version = player->protocol_version;
	}

	try {
		(this->*handler.func)(peer_id, pkt);
	} catch (std::out_of_range &e) {
		Player *player = getPlayerNoLock(peer_id);
		fprintf(stderr, "Server: Packet out_of_range. action=%d, player=%s, msg=%s\n",
			action,
			player ? player->name.c_str() : "(null)",
			e.what()
		);
	} catch (std::exception &e) {
		Player *player = getPlayerNoLock(peer_id);
		fprintf(stderr, "Server: Packet exception. action=%d, player=%s, msg=%s\n",
			action,
			player ? player->name.c_str() : "(null)",
			e.what()
		);
	}
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
