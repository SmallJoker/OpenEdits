#include "client.h"
#include "localplayer.h"
#include "core/packet.h"

const ClientPacketHandler Client::packet_actions[] = {
	{ false, &Client::pkt_Quack }, // 0
	{ false, &Client::pkt_Hello },
	{ false, &Client::pkt_Error },
	{ false, &Client::pkt_GotLobby },
	{ false, &Client::pkt_Join },
	{ true,  &Client::pkt_Leave }, // 5
	{ true,  &Client::pkt_Move },
	{ true,  &Client::pkt_Chat },
	{ false, 0 }
};

void Client::pkt_Quack(Packet &pkt)
{
	printf("Client: Got packet %s\n", pkt.dump().c_str());
}

void Client::pkt_Hello(Packet &pkt)
{
	m_protocol_version = pkt.read<uint16_t>();
	m_my_peer_id = pkt.read<peer_t>();

	printf("Client: hello. my peer_id=%d\n", m_my_peer_id);
}

void Client::pkt_Error(Packet &pkt)
{
	std::string str(pkt.readStr16());
	printf("Client received error: %s\n", str.c_str());
}

void Client::pkt_GotLobby(Packet &pkt)
{
}

void Client::pkt_Join(Packet &pkt)
{
	std::string world_id(pkt.readStr16());
}

void Client::pkt_Leave(Packet &pkt)
{
}

void Client::pkt_Move(Packet &pkt)
{
}

void Client::pkt_Chat(Packet &pkt)
{
}

void Client::pkt_Deprecated(Packet &pkt)
{
	printf("Ignoring deprecated packet %s\n", pkt.dump().c_str());
}
