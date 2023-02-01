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

void Client::pkt_Quack(peer_t peer_id, Packet &pkt)
{
	printf("Client: Got %zu bytes from peer_id=%u\n", pkt.size(), peer_id);
}

void Client::pkt_Hello(peer_t peer_id, Packet &pkt)
{
	printf("Server says hello\n");
}

void Client::pkt_Error(peer_t peer_id, Packet &pkt)
{
	std::string str(pkt.readStr16());
	printf("Client received error: %s\n", str.c_str());
}


void Client::pkt_Join(peer_t peer_id, Packet &pkt)
{
	worldid_t world_id = pkt.read<worldid_t>();
}

void Client::pkt_Leave(peer_t peer_id, Packet &pkt)
{
}

void Client::pkt_Move(peer_t peer_id, Packet &pkt)
{
}

void Client::pkt_Deprecated(peer_t peer_id, Packet &pkt)
{
	std::string name = "??";

	auto player = getPlayer(peer_id);
	if (player)
		name = player->name;

	printf("Ignoring deprecated packet from player %s, peer_id=%u\n",
		name.c_str(), peer_id);
}
