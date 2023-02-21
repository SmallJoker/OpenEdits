#include "client.h"
#include "localplayer.h"
#include "core/connection.h"
#include "core/packet.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Client::Client(ClientStartData &init)
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

	m_nickname = init.nickname;
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
	while (world.ptr()) { // run once
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
				it->pos.X, it->pos.Y, it->id);

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
	if (world.ptr()) {
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
		for (auto &kdata : meta.keys) {
			kdata.step(dtime);
		}

		// Process triggers
		bool trigger_event = false;
		Packet pkt;
		pkt.write(Packet2Server::TriggerBlocks);

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
					if (kdata.trigger(-1.0f)) {
						pkt.write(bp.X);
						pkt.write(bp.Y);
					}
				}
				break;
				case Block::ID_COIN:
				case Block::ID_SECRET:
				if (!b.param1) {
					b.param1 = 1;
					world->setBlock(bp, b);
					trigger_event = true;
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
		case E::G2C_LOBBY_REQUEST:
			{
				Packet pkt;
				pkt.write(Packet2Server::GetLobby);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_JOIN:
			if (getWorld().ptr()) {
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
		case E::G2C_LEAVE:
			if (!getWorld().ptr()) {
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

	bool is_ok = world->isValidPosition(bu.pos.X, bu.pos.Y);
	if (!is_ok)
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
	pkt.writeStr16(m_nickname);
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

