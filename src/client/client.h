#pragma once

#include "core/auth.h"
#include "core/environment.h"
#include "core/friends.h"
#include "core/timer.h"
#include "gameevent.h"
#include <string>

class ClientMedia;
class Connection;
class LocalPlayer;
class ClientScript;

struct ClientPacketHandler;
enum class Packet2Server : uint16_t;

// Similar to RemotePlayerState
enum class ClientState {
	Invalid,
	None,
	Connected,
	Register,
	MediaDownload,
	LobbyIdle,
	WorldJoin,
	WorldPlay
};

struct ClientStartData {
	std::string address;
	std::string nickname;
	std::string password;
	std::string world_id;
};

// Abstract for inheritance
class Client : public Environment, public GameEventHandler {
public:
	/// "init" is invalid afterwards!
	Client(ClientStartData &init);
	~Client();

	void prepareScript(ClientScript *script, bool need_audiovisuals);
	void connect();

	void step(float dtime) override;
	bool OnEvent(GameEvent &e) override;

	// ----------- Functions for the GUI -----------

	/// Main purpose: client-sided information display
	std::string getDebugInfo();
	const ClientStartData &getStartData() { return m_start_data; }

	PtrLock<LocalPlayer> getMyPlayer();
	peer_t getMyPeerId() { return m_my_peer_id; }
	PtrLock<decltype(m_players)> getPlayerList();
	RefCnt<World> getWorld();
	bool updateBlock(BlockUpdate bu);

	void clearTileCacheFor(bid_t block_id);

	std::vector<LobbyWorld> world_list;
	std::vector<LobbyFriend> friend_list;

	// ----------- Utility functions -----------

	LocalPlayer *getPlayerNoLock(peer_t peer_id);
	ClientState getState() const { return m_state; }

	// ----------- Networking -----------
	void disconnect(const char *reason);
	Packet createPacket(Packet2Server type) const;
	void sendPlayerMove();

	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

private:
	void stepPhysics(float dtime);

	void initScript();

	uint8_t getBlockTile(const Player *player, const Block *b);

	/// Updates all tiles
	void updateWorld(bool reset_tiles);

	void quitToLobby(LocalPlayer *p_to_keep);

	void pkt_Quack(Packet &pkt);
	void pkt_Hello(Packet &pkt);
	void pkt_Message(Packet &pkt);
	void pkt_Auth(Packet &pkt);
	void pkt_MediaList(Packet &pkt);
	void pkt_MediaReceive(Packet &pkt);
	void pkt_Lobby(Packet &pkt);
	void pkt_WorldData(Packet &pkt);
	void pkt_Join(Packet &pkt);
	void pkt_Leave(Packet &pkt);
	void pkt_SetPosition(Packet &pkt);
	void pkt_Move(Packet &pkt);
	void pkt_Chat(Packet &pkt);
	void pkt_ChatReplay(Packet &pkt);
	void pkt_PlaceBlock(Packet &pkt);
	void pkt_ScriptEvent(Packet &pkt);
	void pkt_ActivateBlock(Packet &pkt);
	void pkt_GodMode(Packet &pkt);
	void pkt_Smiley(Packet &pkt);
	void pkt_PlayerFlags(Packet &pkt);
	void pkt_WorldMeta(Packet &pkt);
	void pkt_Deprecated(Packet &pkt);

	static const ClientPacketHandler packet_actions[];

	uint64_t m_time = 0,
		m_time_prev = 0; // old time, before step() call

	Timer m_pos_send_timer;
	RateLimit m_rl_scriptevents;

	// State used for packet filtering
	ClientState m_state = ClientState::None;
	uint16_t m_protocol_version = 0;

	std::string m_world_id = "foobar";
	ClientStartData m_start_data;
	peer_t m_my_peer_id = 0;

	std::map<size_t, uint8_t> m_tiles_cache;
	bool m_tiles_map_dirty = false;

	Auth m_auth;
	ClientMedia *m_media = nullptr;
	ClientScript *m_script = nullptr;
};

struct ClientPacketHandler {
	ClientState min_player_state;
	void (Client::*func)(Packet &pkt);
};
