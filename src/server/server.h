#pragma once

#include "core/connection.h"
#include "remoteplayer.h"
#include <map>

//enum class EventType;
struct ServerPacketAction;


class Server : public PacketProcessor {
public:
	Server();

	// ----------- Networking -----------
	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

	void pkt_Hello(RemotePlayer &player, Packet &pkt);
	void pkt_Join(RemotePlayer &player, Packet &pkt);
	void pkt_Leave(RemotePlayer &player, Packet &pkt);
	void pkt_Move(RemotePlayer &player, Packet &pkt);
	void pkt_Deprecated(RemotePlayer &player, Packet &pkt);

private:
	static const ServerPacketAction packet_actions[];
	Connection m_con;

	std::map<peer_t, RemotePlayer> m_players;
};

struct ServerPacketAction {
	int event;
	void (Server::*handler)(RemotePlayer &player, Packet &pkt);
};
