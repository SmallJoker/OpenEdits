#pragma once

#include "core/environment.h"
#include <map>

class RemotePlayer;
struct ServerPacketHandler;


class Server : public Environment {
public:
	Server();
	~Server();

	// ----------- Utility functions -----------
	RemotePlayer *getPlayer(peer_t peer_id);

	// ----------- Networking -----------
	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

	void pkt_Quack(peer_t peer_id, Packet &pkt);
	void pkt_Hello(peer_t peer_id, Packet &pkt);
	void pkt_Join(peer_t peer_id, Packet &pkt);
	void pkt_Leave(peer_t peer_id, Packet &pkt);
	void pkt_Move(peer_t peer_id, Packet &pkt);
	void pkt_Deprecated(peer_t peer_id, Packet &pkt);

	void sendError(peer_t peer_id, const std::string &text);

private:
	static const ServerPacketHandler packet_actions[];

	std::map<worldid_t, World *> m_worlds;
};

struct ServerPacketHandler {
	bool needs_player;
	void (Server::*func)(peer_t peer_id, Packet &pkt);
};
