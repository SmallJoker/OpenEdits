#pragma once

#include "core/chatcommand.h"
#include "core/environment.h"
#include "core/types.h" // RefCnt

enum class RemotePlayerState;

class ChatCommand;
class DatabaseAuth;
class DatabaseWorld;
class RemotePlayer;
struct ServerPacketHandler;


class Server : public Environment {
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

	static const ServerPacketHandler packet_actions[];

	void writeWorldData(Packet &out, World &world, bool is_clear);
	void respawnPlayer(Player *player, bool send_packet);

	DatabaseAuth *m_auth_db = nullptr;
	DatabaseWorld *m_world_db = nullptr;

	// ----------- Chat commands -----------
	void systemChatSend(Player *player, const std::string &msg);

	CHATCMD_FUNC(chat_Help);
	CHATCMD_FUNC(chat_SetPass);
	CHATCMD_FUNC(chat_Flags);
	CHATCMD_FUNC(chat_FFilter);
	bool changePlayerFlags(Player *player, std::string msg, bool do_add);
	CHATCMD_FUNC(chat_FSet);
	CHATCMD_FUNC(chat_FDel);
	CHATCMD_FUNC(chat_Respawn);
	CHATCMD_FUNC(chat_Clear);
	CHATCMD_FUNC(chat_Import);
	CHATCMD_FUNC(chat_Save);

	ChatCommand m_chatcmd;
};


struct ServerPacketHandler {
	RemotePlayerState min_player_state;
	void (Server::*func)(peer_t peer_id, Packet &pkt);
};
