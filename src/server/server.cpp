#include "server.h"
#include "remoteplayer.h"
#include "core/packet.h"

static uint16_t PACKET_ACTIONS_MAX; // initialized in ctor

Server::Server()
{
	puts("Server: startup");
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
	puts("Server: stopping...");

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
	{
		Packet pkt;
		pkt.write<Packet2Client>(Packet2Client::Quack);
		pkt.writeStr16("hello world");
		m_con->send(peer_id, 0, pkt);
	}

	{
		Packet pkt;
		pkt.write<Packet2Client>(Packet2Client::Error);
		pkt.writeStr16("No error. Everything's fine");
		m_con->send(peer_id, 0, pkt);
	}
}

void Server::onPeerDisconnected(peer_t peer_id)
{
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

	if (handler.needs_player) {
		RemotePlayer *player = getPlayer(peer_id);
		if (!player) {
			printf("Server: Player peer_id= %d not found.\n", peer_id);
			return;
		}
	}

	try {
		(this->*handler.func)(peer_id, pkt);
	} catch (std::out_of_range &e) {
		printf("Server: Action %d parsing error: %s\n", action, e.what());
	} catch (std::exception &e) {
		printf("Server: Action %d general error: %s\n", action, e.what());
	}
}
