#pragma once

#include "core/chatcommand.h"
#include "core/environment.h"
#include "core/playerflags.h"
#include "core/timer.h"
#include "core/types.h" // RefCnt

enum class RemotePlayerState;

class DatabaseAuth;
class DatabaseWorld;
class RemotePlayer;
struct ServerPacketHandler;
struct LobbyWorld;


class Server : public Environment, public ChatCommandHandler {
public:
	Server();
	~Server();

	void step(float dtime) override;

	// ----------- Utility functions -----------
	RemotePlayer *getPlayerNoLock(peer_t peer_id);
	RefCnt<World> getWorldNoLock(std::string &id);

	// ----------- Networking -----------
	void onPeerConnected(peer_t peer_id) override;
	void onPeerDisconnected(peer_t peer_id) override;
	void processPacket(peer_t peer_id, Packet &pkt) override;

private:
	void pkt_Quack(peer_t peer_id, Packet &pkt);
	void pkt_Hello(peer_t peer_id, Packet &pkt);
	void pkt_Auth(peer_t peer_id, Packet &pkt);
	void pkt_GetLobby(peer_t peer_id, Packet &pkt);
	void pkt_Join(peer_t peer_id, Packet &pkt);
	void pkt_Leave(peer_t peer_id, Packet &pkt);
	void pkt_Move(peer_t peer_id, Packet &pkt);
	void pkt_Chat(peer_t peer_id, Packet &pkt);
	void pkt_PlaceBlock(peer_t peer_id, Packet &pkt);
	void pkt_TriggerBlocks(peer_t peer_id, Packet &pkt);
	void pkt_GodMode(peer_t peer_id, Packet &pkt);
	void pkt_Smiley(peer_t peer_id, Packet &pkt);
	void pkt_Deprecated(peer_t peer_id, Packet &pkt);

	void sendMsg(peer_t peer_id, const std::string &text);

	void broadcastInWorld(Player *player, int flags, Packet &pkt);

	#define SERVER_PKT_CB [&](Packet &out, u16 proto_ver) -> void
	void broadcastInWorld(Player *player, RemotePlayerState min_state,
		int flags, std::function<void(Packet &, u16)> cb);

	static const ServerPacketHandler packet_actions[];

	RefCnt<World> loadWorldNoLock(const std::string &id);
	void writeWorldData(Packet &out, World &world, bool is_clear);
	void respawnPlayer(Player *player, bool send_packet);

	// ----------- Server checks -----------

	static bool checkSize(std::string &out, blockpos_t size);
	static bool checkTitle(std::string &out, std::string &title);

	// ----------- Other members -----------
	DatabaseAuth *m_auth_db = nullptr;
	DatabaseWorld *m_world_db = nullptr;


	Timer m_ban_cleanup_timer;
	Timer m_stdout_flush_timer;

	// ----------- World imports -----------

	std::map<std::string, LobbyWorld> m_importable_worlds;
	Timer m_importable_worlds_timer;

	// ----------- Chat commands -----------
	void registerChatCommands();
	void systemChatSend(Player *player, const std::string &msg);

	CHATCMD_FUNC(chat_Help);
	CHATCMD_FUNC(chat_SetPass);
	CHATCMD_FUNC(chat_SetCode);
	CHATCMD_FUNC(chat_Code);
	CHATCMD_FUNC(chat_Flags);
	CHATCMD_FUNC(chat_FFilter);
	bool changePlayerFlags(Player *player, std::string msg, bool do_add);
	void handlePlayerFlagsChange(Player *player, playerflags_t flags_mask);
	CHATCMD_FUNC(chat_FSet);
	CHATCMD_FUNC(chat_FDel);
	CHATCMD_FUNC(chat_Respawn);
	CHATCMD_FUNC(chat_Clear);
	CHATCMD_FUNC(chat_Import);
	CHATCMD_FUNC(chat_Load);
	CHATCMD_FUNC(chat_Save);
	CHATCMD_FUNC(chat_Title);

	ChatCommand m_chatcmd;
};


struct ServerPacketHandler {
	RemotePlayerState min_player_state;
	void (Server::*func)(peer_t peer_id, Packet &pkt);
};
