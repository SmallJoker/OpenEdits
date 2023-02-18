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

	// ----------- Functions for the GUI -----------
	PtrLock<LocalPlayer> getMyPlayer();
	PtrLock<decltype(m_players)> getPlayerList();
	RefCnt<World> getWorld();
	bool updateBlock(const BlockUpdate bu);

	std::map<std::string, LobbyWorld> world_list;

	// ----------- Utility functions -----------

	LocalPlayer *getPlayerNoLock(peer_t peer_id);
	ClientState getState() { return m_state; }

	// ----------- Networking -----------
	void sendPlayerMove();

	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

private:
	void pkt_Quack(Packet &pkt);
	void pkt_Hello(Packet &pkt);
	void pkt_Error(Packet &pkt);
	void pkt_Lobby(Packet &pkt);
	void pkt_WorldData(Packet &pkt);
	void pkt_Join(Packet &pkt);
	void pkt_Leave(Packet &pkt);
	void pkt_SetPosition(Packet &pkt);
	void pkt_Move(Packet &pkt);
	void pkt_Chat(Packet &pkt);
	void pkt_PlaceBlock(Packet &pkt);
	void pkt_Key(Packet &pkt);
	void pkt_GodMode(Packet &pkt);
	void pkt_Deprecated(Packet &pkt);

	static const ClientPacketHandler packet_actions[];

	// State used for packet filtering
	ClientState m_state = ClientState::None;
	uint16_t m_protocol_version = 0;

	std::string m_world_id = "foobar";
	std::string m_nickname;
	peer_t m_my_peer_id = 0;
};

struct ClientPacketHandler {
	ClientState min_player_state;
	void (Client::*func)(Packet &pkt);
};
