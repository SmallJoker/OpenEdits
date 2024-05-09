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
	Server(bool *shutdown_requested);
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
	void signInPlayer(RemotePlayer *player);
	void pkt_Auth(peer_t peer_id, Packet &pkt);
	void pkt_MediaRequest(peer_t peer_id, Packet &pkt);
	void pkt_GetLobby(peer_t peer_id, Packet &pkt);
	void pkt_Join(peer_t peer_id, Packet &pkt);
	void pkt_Leave(peer_t peer_id, Packet &pkt);
	void pkt_Move(peer_t peer_id, Packet &pkt);
	void pkt_Chat(peer_t peer_id, Packet &pkt);
	void pkt_PlaceBlock(peer_t peer_id, Packet &pkt);
	void pkt_TriggerBlocks(peer_t peer_id, Packet &pkt);
	void pkt_GodMode(peer_t peer_id, Packet &pkt);
	void pkt_Smiley(peer_t peer_id, Packet &pkt);
	void pkt_FriendAction(peer_t peer_id, Packet &pkt);
	void pkt_Deprecated(peer_t peer_id, Packet &pkt);

	void sendMsg(peer_t peer_id, const std::string &text);

	void broadcastInWorld(Player *player, int flags, Packet &pkt);
	void broadcastInWorld(const World *world, int flags, Packet &pkt);

	#define SERVER_PKT_CB [&](Packet &out) -> void
	void broadcastInWorld(Player *player, RemotePlayerState min_state,
		int flags, std::function<void(Packet &)> cb);

	static const ServerPacketHandler packet_actions[];

	bool loadWorldNoLock(World *world);
	void writeWorldData(Packet &out, World &world, bool is_clear);
	void setDefaultPlayerFlags(Player *player);
	void teleportPlayer(Player *player, core::vector2df dst, bool reset_progress = false);
	void respawnPlayer(Player *player, bool send_packet, bool reset_progress = true);

	// ----------- Server checks -----------

	static bool checkSize(std::string &out, blockpos_t size);
	static bool checkTitle(std::string &out, std::string &title);

	// ----------- Other members -----------
	DatabaseAuth *m_auth_db = nullptr;
	DatabaseWorld *m_world_db = nullptr;
	bool m_is_first_step = true;

	std::map<peer_t, Timer> m_deaths;

	Timer m_ban_cleanup_timer;
	Timer m_stdout_flush_timer;

	bool *m_shutdown_requested;
	Timer m_shutdown_timer;
	Timer m_shutdown_timer_remind;

	// ----------- World imports -----------

	std::map<std::string, LobbyWorld> m_importable_worlds;
	Timer m_importable_worlds_timer;

	// ----------- Chat commands -----------
	void registerChatCommands();
	void systemChatSend(Player *player, const std::string &msg, bool broadcast = false);
	/// We might pass "world == nullptr" by accident, thus
	/// have a separate "any_world" option is safer.
	Player *findPlayer(const World *world, std::string name, bool any_world = false);

	CHATCMD_FUNC(chat_Help);
	CHATCMD_FUNC(chat_Shutdown);
	CHATCMD_FUNC(chat_SetPass);
	CHATCMD_FUNC(chat_SetCode);
	CHATCMD_FUNC(chat_Code);
	CHATCMD_FUNC(chat_Dev);
	CHATCMD_FUNC(chat_Ban);
	CHATCMD_FUNC(chat_Flags);
	CHATCMD_FUNC(chat_FFilter);
	/// Which flags that "actor" may change on "target"
	playerflags_t mayManipulatePlayer(Player *actor, Player *target);
	bool changePlayerFlags(Player *player, std::string msg, bool do_add);
	void handlePlayerFlagsChange(Player *player, playerflags_t flags_mask);
	CHATCMD_FUNC(chat_FSet);
	CHATCMD_FUNC(chat_FDel);
	CHATCMD_FUNC(chat_Respawn);
	CHATCMD_FUNC(chat_Teleport);
	CHATCMD_FUNC(chat_Clear);
	CHATCMD_FUNC(chat_Import);
	/// Players must already have joined the new world
	void sendPlayerFlags(const World *world);
	CHATCMD_FUNC(chat_Load);
	CHATCMD_FUNC(chat_Save);
	CHATCMD_FUNC(chat_Title);

	ChatCommand m_chatcmd;
};


struct ServerPacketHandler {
	RemotePlayerState min_player_state;
	void (Server::*func)(peer_t peer_id, Packet &pkt);
};
