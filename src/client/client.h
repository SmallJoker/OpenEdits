#pragma once

#include "core/environment.h"
#include "gameevent.h"
#include <string>

class LocalPlayer;
class Connection;

struct ClientPacketHandler;

// Similar to RemotePlayerState
enum class ClientState {
	Invalid,
	None,
	Connected,
	LobbyIdle,
	WorldJoin,
	WorldPlay
};

struct ClientStartData {
	std::string address;
	std::string nickname;
};

struct PlayerControls {
	core::vector2di direction;
	bool jump = false;
	bool shift = false;
};

struct LobbyWorld : public WorldMeta {
	blockpos_t size;
};

// Abstract for inheritance
class Client : public Environment, public GameEventHandler {
public:
	Client(ClientStartData &init);
	~Client();

	void step(float dtime) override;
	bool OnEvent(GameEvent &e) override;

	// ----------- Utility functions -----------
	PtrLock<LocalPlayer> getMyPlayer();
	PlayerControls &getControls() { return m_controls; }
	World *getWorld() { return m_world; }
	bool setBlock(blockpos_t pos, Block block, char layer = 0);

	LocalPlayer *getPlayerNoLock(peer_t peer_id);
	ClientState getState() { return m_state; }

	// ----------- Networking -----------
	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

	void pkt_Quack(Packet &pkt);
	void pkt_Hello(Packet &pkt);
	void pkt_Error(Packet &pkt);
	void pkt_Lobby(Packet &pkt);
	void pkt_WorldData(Packet &pkt);
	void pkt_Join(Packet &pkt);
	void pkt_Leave(Packet &pkt);
	void pkt_Move(Packet &pkt);
	void pkt_Chat(Packet &pkt);
	void pkt_PlaceBlock(Packet &pkt);
	void pkt_Deprecated(Packet &pkt);

	std::map<std::string, LobbyWorld> world_list;

protected:
	ClientState m_state = ClientState::None;
	uint16_t m_protocol_version = 0;

	World *m_world = nullptr;
	PlayerControls m_controls;

	std::string m_world_hash = "foobar";
	std::string m_nickname;
	peer_t m_my_peer_id = 0;

private:
	static const ClientPacketHandler packet_actions[];
};

struct ClientPacketHandler {
	ClientState min_player_state;
	void (Client::*func)(Packet &pkt);
};
