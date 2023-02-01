#pragma once

#include "core/environment.h"
#include <string>

class LocalPlayer;
class Connection;
class GameEventHandler;

struct ClientPacketHandler;

struct ClientStartData {
	std::string address;
	std::string nickname;
};

// Abstract for inheritance
class Client : public Environment {
public:
	Client(ClientStartData &init);
	~Client();

	void setEventHandler(GameEventHandler *gui) { m_handler = gui; }

	// ----------- Utility functions -----------
	LocalPlayer *getPlayer(peer_t peer_id);

	// ----------- Networking -----------
	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

	void pkt_Hello(peer_t peer_id, Packet &pkt);
	void pkt_Join(peer_t peer_id, Packet &pkt);
	void pkt_Leave(peer_t peer_id, Packet &pkt);
	void pkt_Move(peer_t peer_id, Packet &pkt);
	void pkt_Deprecated(peer_t peer_id, Packet &pkt);

protected:
	World *m_world = nullptr;

	std::string m_world_hash = "foobar";
	peer_t m_local_peer = 0;

	GameEventHandler *m_handler = nullptr;

private:
	static const ClientPacketHandler packet_actions[];
};

struct ClientPacketHandler {
	int action;
	bool needs_player;
	void (Client::*func)(peer_t peer_id, Packet &pkt);
};
