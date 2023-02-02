#include "client.h"
#include "localplayer.h"
#include "core/packet.h"

const ClientPacketHandler Client::packet_actions[] = {
	{ false, &Client::pkt_Quack }, // 0
	{ false, &Client::pkt_Hello },
	{ false, &Client::pkt_Error },
	{ false, &Client::pkt_Join },
	{ true,  &Client::pkt_Leave },
	{ true,  &Client::pkt_Move }, // 5
	{ false, 0 }
};

void Client::pkt_Quack(Packet &pkt)
{
	printf("Client: Got packet %s\n", pkt.dump().c_str());
}

void Client::pkt_Hello(Packet &pkt)
{
	m_my_peer_id = pkt.read<peer_t>();

	printf("Client: hello. my peer_id=%d\n", m_my_peer_id);
}

void Client::pkt_Error(Packet &pkt)
{
	std::string str(pkt.readStr16());
	printf("Client received error: %s\n", str.c_str());
}

void Client::pkt_Join(Packet &pkt)
{
	worldid_t world_id = pkt.read<worldid_t>();
}

void Client::pkt_Leave(Packet &pkt)
{
}

void Client::pkt_Move(Packet &pkt)
{
}

void Client::pkt_Deprecated(Packet &pkt)
{
	printf("Ignoring deprecated packet %s\n", pkt.dump().c_str());
}
