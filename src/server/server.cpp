#include "server.h"
#include "core/packet.h"

uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Server::Server() :
	m_con(Connection::TYPE_SERVER)
{
	m_con.listenAsync(*this);

	PACKET_ACTIONS_MAX = 0;
	const ServerPacketAction *action = packet_actions;
	while (action->handler)
		action++;

	PACKET_ACTIONS_MAX = action - packet_actions;
}


// -------------- Public members -------------

void Server::onPeerConnected(peer_t peer_id)
{
}

void Server::onPeerDisconnected(peer_t peer_id)
{
}

void Server::processPacket(peer_t peer_id, Packet &pkt)
{
	// one server instance, multiple worlds
	uint16_t action = pkt.read<uint16_t>();
	if (action >= PACKET_ACTIONS_MAX) {
		puts("Packet action out of range");
		return;
	}

	auto it = m_players.find(peer_id);
	if (it == m_players.end()) {
		puts("Player not found");
		return;
	}

	try {
		(this->*packet_actions[action].handler)(it->second, pkt);
	} catch (std::out_of_range &e) {
		printf("Action %d parsing error: %s\n", action, e.what());
	} catch (std::exception &e) {
		printf("Action %d general error: %s\n", action, e.what());
	}
}
