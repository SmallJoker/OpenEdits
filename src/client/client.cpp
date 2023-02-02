#include "client.h"
#include "localplayer.h"
#include "core/connection.h"
#include "core/events.h"
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
	}

	m_nickname = init.nickname;
}

Client::~Client()
{
	puts("Client: stopping...");

	for (auto it : m_players)
		delete it.second;
	m_players.clear();

	delete m_con;
}

// -------------- Public members -------------

void Client::step(float dtime)
{
	// run player physics
	// process user inputs
}


LocalPlayer *Client::getPlayer(peer_t peer_id)
{
	// It's not really a "peer" ID but player ID
	auto it = m_players.find(peer_id);
	return it != m_players.end() ? dynamic_cast<LocalPlayer *>(it->second) : nullptr;
}


void Client::onPeerConnected(peer_t peer_id)
{
	m_is_connected = true;
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
	m_is_connected = false;

	// send event for client destruction
	if (m_handler) {
		GameEvent e;
		e.type = GameEvent::GE_WORLD_LEAVE;
		e.str = "Server disconnected";
		m_handler->OnEvent(e);
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

	try {
		(this->*handler.func)(pkt);
	} catch (std::out_of_range &e) {
		printf("Client: Action %d parsing error: %s\n", action, e.what());
	} catch (std::exception &e) {
		printf("Client: Action %d general error: %s\n", action, e.what());
	}
}

