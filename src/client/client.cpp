#include "client.h"
#include "localplayer.h"
#include "core/auth.h"
#include "core/connection.h"
#include "core/packet.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor
extern BlockManager *g_blockmanager; // for client-only use

Client::Client(ClientStartData &init) :
	Environment(g_blockmanager),
	m_start_data(init)
{
	puts("Client: startup");

	m_con = new Connection(Connection::TYPE_CLIENT, "Client");
	m_con->connect(init.address.c_str());
	m_con->listenAsync(*this);

	{
		PACKET_ACTIONS_MAX = 0;
		const ClientPacketHandler *handler = packet_actions;
		while (handler->func)
			handler++;

		PACKET_ACTIONS_MAX = handler - packet_actions;
		ASSERT_FORCED(PACKET_ACTIONS_MAX == (int)Packet2Client::MAX_END, "Packet handler mismatch");
	}
}

Client::~Client()
{
	puts("Client: stopping...");

	{
		// In case a packet is being processed
		SimpleLock lock(m_players_lock);

		for (auto it : m_players)
			delete it.second;
		m_players.clear();
	}

	delete m_con;
}

// -------------- Public members -------------

void Client::step(float dtime)
{
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
		for (auto it = queue.cbegin(); it != queue.cend();) {

			out.write<u8>(true); // begin
			// Write BlockUpdate
			it->write(out);

			DEBUGLOG("Client: sending block x=%d,y=%d,id=%d\n",
				it->pos.X, it->pos.Y, it->getId());

			it = queue.erase(it);

			// Fit everything into an MTU
			if (out.size() > CONNECTION_MTU)
				break;
		}
		out.write<u8>(false);

		m_con->send(0, 0, out);
		break;
	}

	// Run physics engine
	if (world.get()) {
		std::set<blockpos_t> triggered_blocks;
		SimpleLock lock(m_players_lock);

		for (auto it : m_players) {
			if (it.first == m_my_peer_id) {
				it.second->triggered_blocks = &triggered_blocks;
				it.second->step(dtime);
				it.second->triggered_blocks = nullptr;
			} else {
				it.second->step(dtime);
			}
		}

		auto &meta = world->getMeta();

		// Process triggers
		bool trigger_event = false;
		Packet pkt;
		pkt.write(Packet2Server::TriggerBlocks);

		auto player = getPlayerNoLock(m_my_peer_id);
		for (blockpos_t bp : triggered_blocks) {
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
				if (!b.tile) {
					b.tile = 1;
					world->setBlock(bp, b);
					trigger_event = true;
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
						trigger_event = true;
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

		if (trigger_event) {
			// Triggering an event anyway. Update coin doors.
			auto player = getPlayerNoLock(m_my_peer_id);
			player->updateCoinCount();

			GameEvent e(GameEvent::C2G_MAP_UPDATE);
			sendNewEvent(e);
		}
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
				// Cannot leave
				return false;
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

PtrLock<LocalPlayer> Client::getMyPlayer()
{
	m_players_lock.lock();
	return PtrLock<LocalPlayer>(m_players_lock, getPlayerNoLock(m_my_peer_id));
}

PtrLock<decltype(Client::m_players)> Client::getPlayerList()
{
	m_players_lock.lock();

#if 0
	std::vector<std::string> list;
	list.reserve(m_players.size());
	for (auto it : m_players) {
		list.push_back(it.second->name);
	}
	return list;
#endif
	return PtrLock<decltype(m_players)>(m_players_lock, &m_players);
}

RefCnt<World> Client::getWorld()
{
	auto player = getPlayerNoLock(m_my_peer_id);
	return player ? player->getWorld() : nullptr;
}


bool Client::updateBlock(const BlockUpdate bu)
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

void Client::sendPlayerMove()
{
	auto player = getMyPlayer();
	if (!player)
		return;

	Packet pkt;
	pkt.write(Packet2Server::Move);
	player->writePhysics(pkt);
	m_con->send(0, 1 | Connection::FLAG_UNRELIABLE, pkt);
}


void Client::onPeerConnected(peer_t peer_id)
{
	m_state = ClientState::Connected;
	puts("Client: hello server!");

	Packet pkt;
	pkt.write(Packet2Server::Hello);
	pkt.write(PROTOCOL_VERSION);
	pkt.write(PROTOCOL_VERSION_MIN);
	pkt.writeStr16(m_start_data.nickname);

	m_con->send(0, 0, pkt);
}

void Client::onPeerDisconnected(peer_t peer_id)
{
	m_state = ClientState::None;

	// send event for client destruction
	{
		GameEvent e2(GameEvent::C2G_DISCONNECT);
		sendNewEvent(e2);
	}
	printf("Client: Disconnected from the server\n");
}

void Client::processPacket(peer_t peer_id, Packet &pkt)
{
	// one server instance, multiple worlds
	int action = (int)pkt.read<Packet2Client>();
	if (action >= PACKET_ACTIONS_MAX) {
		printf("Client: Packet action %u out of range: %s\n", action, pkt.dump(20).c_str());
		return;
	}

	const ClientPacketHandler &handler = packet_actions[action];
	if ((int)handler.min_player_state > (int)m_state) {
		printf("Client: not ready for action=%d.\n", action);
		return;
	}

	try {
		(this->*handler.func)(pkt);
	} catch (std::out_of_range &e) {
		printf("Client: Action %d parsing error: %s\n", action, e.what());
	} catch (std::exception &e) {
		printf("Client: Action %d general error: %s\n", action, e.what());
	}
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
			return player->godmode;
		case Block::ID_COINDOOR:
		case Block::ID_COINGATE:
			return get_params().param_u8 >= player->coins;
		case Block::ID_DOOR_R:
		case Block::ID_DOOR_G:
		case Block::ID_DOOR_B:
			return world->getMeta().keys[b->id - Block::ID_DOOR_R].isActive();
		case Block::ID_GATE_R:
		case Block::ID_GATE_G:
		case Block::ID_GATE_B:
			return world->getMeta().keys[b->id - Block::ID_GATE_R].isActive();
	}
	return 0;
}

void Client::updateWorld()
{
	auto player = getPlayerNoLock(m_my_peer_id);
	auto world = player->getWorld();

	for (Block *b = world->begin(); b < world->end(); ++b) {
		switch (b->id) {
			case Block::ID_SPIKES:
				{
					BlockParams params;
					world->getParams(world->getBlockPos(b), &params);
					b->tile = params.param_u8;
				}
				break;
		}
	}
}

