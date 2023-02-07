#include "client.h"
#include "localplayer.h"
#include "core/connection.h"
#include "core/packet.h"

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
	// run player physics
	// process user inputs
	while (m_world) {
		SimpleLock lock(m_world->mutex);
		auto &queue = m_world->proc_queue;
		if (queue.empty())
			break;

		Packet out;
		out.write(Packet2Server::PlaceBlock);

		// Almost identical to the queue processing in Server::step()
		for (auto it = queue.cbegin(); it != queue.cend();) {

			out.write<u8>(true); // begin
			// blockpos_t
			out.write(it->first.X);
			out.write(it->first.Y);
			// Block
			out.write(it->second.id);
			out.write(it->second.param1);

			printf("Client: sending block x=%d,y=%d,id=%d\n",
				it->first.X, it->first.Y, it->second.id);

			it = queue.erase(it);

			// Fit everything into an MTU
			if (out.size() > CONNECTION_MTU)
				break;
		}
		out.write<u8>(false);

		m_con->send(0, 0, out);
		break;
	}
}

bool Client::OnEvent(GameEvent &e)
{
	using E = GameEvent::G2C_Enum;

	switch (e.type_g2c) {
		case E::G2C_INVALID:
			return false;
		case E::G2C_JOIN:
			{
				m_state = ClientState::WorldJoin;

				Packet pkt;
				pkt.write(Packet2Server::Join);
				pkt.writeStr16(*e.text);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_LEAVE:
			m_con->disconnect(0);
			return true;
		case E::G2C_CHAT:
			{
				Packet pkt;
				pkt.write(Packet2Server::Chat);
				pkt.writeStr16(*e.text);
				m_con->send(0, 0, pkt);
			}
			return true;
		case E::G2C_SET_BLOCK:
			return true;
	}
	return false;
}

PtrLock<LocalPlayer> Client::getMyPlayer()
{
	m_players_lock.lock();
	return PtrLock<LocalPlayer>(m_players_lock, getPlayerNoLock(m_my_peer_id));
}

LocalPlayer *Client::getPlayerNoLock(peer_t peer_id)
{
	// It's not really a "peer" ID but player ID
	auto it = m_players.find(peer_id);
	return it != m_players.end() ? dynamic_cast<LocalPlayer *>(it->second) : nullptr;
}

bool Client::setBlock(blockpos_t pos, Block block, char layer)
{
	if (!m_world)
		return false;

	SimpleLock lock(m_world->mutex);

	bool is_ok = m_world->setBlock(pos, block, layer);
	if (!is_ok)
		return false;

	BlockUpdate bu;
	bu.id = block.id;
	bu.param1 = block.param1;

	m_world->proc_queue.emplace(pos, bu);
	return true;
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
	if (m_eventhandler) {
		GameEvent e(GameEvent::C2G_DIALOG);
		e.text = new std::string("Server disconnected");
		sendNewEvent(e);

		GameEvent e2(GameEvent::C2G_DISCONNECT);
		sendNewEvent(e2);
	}
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

