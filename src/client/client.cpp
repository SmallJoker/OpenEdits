#include "client.h"
#include "localplayer.h"
#include "core/connection.h"
#include "core/events.h"
#include "core/packet.h"

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Client::Client(ClientStartData &init)
{
	m_con = new Connection(Connection::TYPE_CLIENT);
	m_con->connect(init.address.c_str());

	PACKET_ACTIONS_MAX = 0;
	const ClientPacketHandler *handler = packet_actions;
	while (handler->func)
		handler++;

	PACKET_ACTIONS_MAX = handler - packet_actions;
	// auth, etc.
}

Client::~Client()
{
	for (auto it : m_players)
		delete it.second;
	m_players.clear();

	delete m_con;
}

// -------------- Public members -------------

LocalPlayer *Client::getPlayer(peer_t peer_id)
{
	// It's not really a "peer" ID but player ID
	auto it = m_players.find(peer_id);
	return it != m_players.end() ? dynamic_cast<LocalPlayer *>(it->second) : nullptr;
}


void Client::onPeerConnected(peer_t peer_id)
{
	puts("Client: hello server!");
}

void Client::onPeerDisconnected(peer_t peer_id)
{
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
	uint16_t action = pkt.read<uint16_t>();
	if (action >= PACKET_ACTIONS_MAX) {
		puts("Packet action out of range");
		return;
	}

	const ClientPacketHandler &handler = packet_actions[action];

	if (handler.needs_player) {
		LocalPlayer *player = getPlayer(peer_id);
		if (!player) {
			puts("Player not found");
			return;
		}
	}

	try {
		(this->*handler.func)(peer_id, pkt);
	} catch (std::out_of_range &e) {
		printf("Action %d parsing error: %s\n", action, e.what());
	} catch (std::exception &e) {
		printf("Action %d general error: %s\n", action, e.what());
	}
}

