#include "server.h"
#include "core/packet.h"

const ServerPacketAction Server::packet_actions[] = {
	{ 0, &Server::pkt_Hello },
	{ 0, 0 }
};

void Server::pkt_Hello(RemotePlayer &player, Packet &pkt)
{
	uint16_t protocol_ver = pkt.read<uint16_t>();
	uint16_t protocol_min = pkt.read<uint16_t>();

	protocol_ver = std::max(PROTOCOL_VERSION, protocol_min);
	if (protocol_ver < protocol_min || protocol_ver < PROTOCOL_VERSION_MIN) {
		printf("Old peer_id=%u tried to connect\n", player.peer_id);
		m_con.disconnect(player.peer_id);
	}

	// TODO player state tracking
	// TODO name duplication/validation check
	player.name = pkt.readStr16();
	printf("Hello from %s\n", player.name.c_str());
}

void Server::pkt_Join(RemotePlayer &player, Packet &pkt)
{
	uint16_t world_id = pkt.read<uint16_t>();
}

void Server::pkt_Leave(RemotePlayer &player, Packet &pkt)
{
}

void Server::pkt_Move(RemotePlayer &player, Packet &pkt)
{
}

void Server::pkt_Deprecated(RemotePlayer &player, Packet &pkt)
{
	printf("Ignoring deprecated packet from player %s, peer_id=%u\n",
		player.name.c_str(), player.peer_id);
}
