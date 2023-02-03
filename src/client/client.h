#pragma once

#include "core/environment.h"
#include "gameevent.h"
#include <string>

class LocalPlayer;
class Connection;

struct ClientPacketHandler;

struct ClientStartData {
	std::string address;
	std::string nickname;
};

struct PlayerControls {
	core::vector2di direction;
	bool jump = false;
	bool shift = false;
};

// Abstract for inheritance
class Client : public Environment, public GameEventHandler {
public:
	Client(ClientStartData &init);
	~Client();

	void step(float dtime) override;
	bool OnEvent(GameEvent &e) override;

	// ----------- Utility functions -----------
	inline LocalPlayer *getMyPlayer() { return getPlayer(m_my_peer_id); }
	PlayerControls &getControls() { return m_controls; }

	LocalPlayer *getPlayer(peer_t peer_id);
	bool isConnected() { return m_is_connected; }

	// ----------- Networking -----------
	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

	void pkt_Quack(Packet &pkt);
	void pkt_Hello(Packet &pkt);
	void pkt_Error(Packet &pkt);
	void pkt_Join(Packet &pkt);
	void pkt_Leave(Packet &pkt);
	void pkt_Move(Packet &pkt);
	void pkt_Deprecated(Packet &pkt);

protected:
	bool m_is_connected = false;

	World *m_world = nullptr;
	PlayerControls m_controls;

	std::string m_world_hash = "foobar";
	std::string m_nickname;
	peer_t m_my_peer_id = 0;

private:
	static const ClientPacketHandler packet_actions[];
};

struct ClientPacketHandler {
	signed char needs_player;
	void (Client::*func)(Packet &pkt);
};
