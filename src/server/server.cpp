#include "server.h"
#include "remoteplayer.h"
#include "core/packet.h"

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Server::Server()
{
	m_con = new Connection(Connection::TYPE_SERVER);
	m_con->listenAsync(*this);

	PACKET_ACTIONS_MAX = 0;
	const ServerPacketHandler *handler = packet_actions;
	while (handler->func)
		handler++;

	PACKET_ACTIONS_MAX = handler - packet_actions;
}

Server::~Server()
{
	for (auto it : m_players)
		delete it.second;
	m_players.clear();

	delete m_con;
}


// -------------- Public members -------------

RemotePlayer *Server::getPlayer(peer_t peer_id)
{
	auto it = m_players.find(peer_id);
	return it != m_players.end() ? dynamic_cast<RemotePlayer *>(it->second) : nullptr;
}

void Server::onPeerConnected(peer_t peer_id)
{
	auto player = new RemotePlayer(peer_id);
	m_players.insert({peer_id, player});
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

	const ServerPacketHandler &handler = packet_actions[action];

	if (handler.needs_player) {
		RemotePlayer *player = getPlayer(peer_id);
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
