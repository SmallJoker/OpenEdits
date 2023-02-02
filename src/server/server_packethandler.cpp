#include "server.h"
#include "remoteplayer.h"
#include "core/packet.h"

// in sync with core/packet.h
const ServerPacketHandler Server::packet_actions[] = {
	{ RemotePlayerState::Invalid,   &Server::pkt_Quack }, // 0
	{ RemotePlayerState::Invalid,   &Server::pkt_Hello },
	{ RemotePlayerState::Idle,      &Server::pkt_GetLobby },
	{ RemotePlayerState::Idle,      &Server::pkt_Join },
	{ RemotePlayerState::WorldJoin, &Server::pkt_Leave },
	{ RemotePlayerState::WorldPlay, &Server::pkt_Move },
	{ RemotePlayerState::Invalid, 0 }
};

void Server::pkt_Quack(peer_t peer_id, Packet &pkt)
{
	printf("Server: Got %zu bytes from peer_id=%u\n", pkt.size(), peer_id);
}

void Server::pkt_Hello(peer_t peer_id, Packet &pkt)
{
	uint16_t protocol_ver = pkt.read<uint16_t>();
	uint16_t protocol_min = pkt.read<uint16_t>();

	protocol_ver = std::min(PROTOCOL_VERSION, protocol_ver);
	if (protocol_ver < protocol_min || protocol_ver < PROTOCOL_VERSION_MIN) {
		printf("Old peer_id=%u tried to connect\n", peer_id);
		m_con->disconnect(peer_id);
	}

	std::string name(pkt.readStr16());
	for (char &c : name)
		c = tolower(c);

	bool ok = true;
	for (auto it : m_players) {
		if (it.second->name == name) {
			ok = false;
			break;
		}
	}

	if (!ok) {
		sendError(peer_id, "Player is already online");
		m_con->disconnect(peer_id);
		return;
	}

	auto player = new RemotePlayer(peer_id, protocol_ver);
	m_players.emplace(peer_id, player);

	player->name = name;
	player->state = RemotePlayerState::Idle;

	{
		// Confirm
		Packet reply;
		reply.write<Packet2Client>(Packet2Client::Hello);
		reply.write<peer_t>(peer_id);

		m_con->send(peer_id, 0, reply);
	}


	// TODO player state tracking
	// TODO name duplication/validation check
	printf("Hello from %s, proto_ver=%d\n", player->name.c_str(), player->protocol_version);
}

void Server::pkt_GetLobby(peer_t peer_id, Packet &pkt)
{

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

void Server::sendError(peer_t peer_id, const std::string &text)
{
	Packet pkt;
	pkt.write<Packet2Client>(Packet2Client::Error);
	pkt.writeStr16(text);
	m_con->send(peer_id, 0, pkt);
}

