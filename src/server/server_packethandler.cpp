#include "server.h"
#include "remoteplayer.h"
#include "core/packet.h"

const ServerPacketHandler Server::packet_actions[] = {
	{ 0, false, &Server::pkt_Hello },
	{ 0, true, &Server::pkt_Join },
	{ 0, true, &Server::pkt_Leave },
	{ 0, false, 0 }
};

void Server::pkt_Hello(peer_t peer_id, Packet &pkt)
{
	uint16_t protocol_ver = pkt.read<uint16_t>();
	uint16_t protocol_min = pkt.read<uint16_t>();

	protocol_ver = std::max(PROTOCOL_VERSION, protocol_min);
	if (protocol_ver < protocol_min || protocol_ver < PROTOCOL_VERSION_MIN) {
		printf("Old peer_id=%u tried to connect\n", peer_id);
		m_con->disconnect(peer_id);
	}

	auto player = new RemotePlayer(peer_id);
	m_players.insert({peer_id, player});

	// TODO player state tracking
	// TODO name duplication/validation check
	player->name = pkt.readStr16();
	printf("Hello from %s\n", player->name.c_str());
}

void Server::pkt_Join(peer_t peer_id, Packet &pkt)
{

}

void Server::pkt_Leave(peer_t peer_id, Packet &pkt)
{

}

void Server::pkt_Move(peer_t peer_id, Packet &pkt)
{

}

void Server::pkt_Deprecated(peer_t peer_id, Packet &pkt)
{
	std::string name = "??";

	auto player = getPlayer(peer_id);
	if (player)
		name = player->name;

	printf("Ignoring deprecated packet from player %s, peer_id=%u\n",
		name.c_str(), peer_id);
}
