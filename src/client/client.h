#pragma once

#include "core/auth.h"
#include "core/environment.h"
#include "core/friends.h"
#include "gameevent.h"
#include <string>

class LocalPlayer;
class Connection;

struct ClientPacketHandler;
enum class Packet2Server : uint16_t;

// Similar to RemotePlayerState
enum class ClientState {
	Invalid,
	None,
	Connected,
	Register,
	LobbyIdle,
	WorldJoin,
	WorldPlay
};

struct ClientStartData {
	std::string address;
	std::string nickname;
	std::string password;
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
	peer_t getMyPeerId() { return m_my_peer_id; }
	PtrLock<decltype(m_players)> getPlayerList();
	RefCnt<World> getWorld();
	bool updateBlock(BlockUpdate bu);

	std::vector<LobbyWorld> world_list;
	std::vector<LobbyFriend> friend_list;

	// ----------- Utility functions -----------

	LocalPlayer *getPlayerNoLock(peer_t peer_id);
	ClientState getState() { return m_state; }

	// ----------- Networking -----------
	Packet createPacket(Packet2Server type);
	void sendPlayerMove();

	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

private:
	void stepPhysics(float dtime);

	uint8_t getBlockTile(const Player *player, const Block *b) const;

	// Prepare for rendering
	void updateWorld();

	void pkt_Quack(Packet &pkt);
	void pkt_Hello(Packet &pkt);
	void pkt_Message(Packet &pkt);
	void pkt_Auth(Packet &pkt);
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
	void pkt_Smiley(Packet &pkt);
	void pkt_PlayerFlags(Packet &pkt);
	void pkt_WorldMeta(Packet &pkt);
	void pkt_ChatReplay(Packet &pkt);
	void pkt_Deprecated(Packet &pkt);

	static const ClientPacketHandler packet_actions[];

	static constexpr uint64_t TIME_RESOLUTION = 100; // divisions per second
	uint64_t m_time = 0,
		m_time_prev = 0; // old time, before step() call

	// State used for packet filtering
	ClientState m_state = ClientState::None;
	uint16_t m_protocol_version = 0;

	std::string m_world_id = "foobar";
	ClientStartData m_start_data;
	peer_t m_my_peer_id = 0;

	Auth m_auth;
};

struct ClientPacketHandler {
	ClientState min_player_state;
	void (Client::*func)(Packet &pkt);
};
