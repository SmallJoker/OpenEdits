#pragma once

#include "core/types.h"
#include <cstdint>
#include <string>

using namespace irr;

class Player;

/*
	Holds callback data of various kinds for stuff that is unique
	to the Client or GUI. I want them to be decoupled, hence this code.
*/
struct GameEvent {
	// GUI --> Client. generally requests to send packets
	enum G2C_Enum {
		G2C_INVALID,
		G2C_REGISTER,
		G2C_LOBBY_REQUEST,
		G2C_FRIEND_ACTION,
		G2C_SET_PASSWORD,
		G2C_JOIN,
		G2C_CREATE_WORLD,
		G2C_LEAVE,
		G2C_CHAT,
		G2C_GODMODE,
		G2C_SMILEY,
		G2C_GET_ASSET_PATH
	} type_g2c = G2C_INVALID;

	// Client --> GUI
	enum C2G_Enum {
		C2G_INVALID,
		C2G_DIALOG,
		C2G_DISCONNECT,
		C2G_LOBBY_UPDATE,
		C2G_CHANGE_PASS,
		C2G_JOIN,
		C2G_LEAVE,
		C2G_MAP_UPDATE,
		C2G_ON_TOUCH_BLOCK,
		C2G_META_UPDATE,
		C2G_PLAYER_JOIN,
		C2G_PLAYER_LEAVE,
		C2G_PLAYER_CHAT,
		C2G_CHAT_HISTORY,
		C2G_PLAYERFLAGS,
		C2G_LOCAL_CHAT,
		C2G_SOUND_PLAY
	} type_c2g = C2G_INVALID;

	GameEvent(G2C_Enum v) : type_g2c(v) {}
	GameEvent(C2G_Enum v) : type_c2g(v) {}
	~GameEvent();

	struct FriendAction {
		int action;
		std::string player_name;
	};

	struct PlayerChat {
		Player *player;
		std::string message;
	};

	struct WorldCreationData {
		s32 mode;
		std::string title, code;
	};

	struct BlockData {
		blockpos_t pos;
		Block b;
	};

	struct PasswordChangeData {
		std::string old_pw;
		std::string new_pw;
	};

	union {
		std::string *text;
		Player *player;
		FriendAction *friend_action;
		PlayerChat *player_chat;
		WorldCreationData *wc_data;
		BlockData *block;
		PasswordChangeData *password;
		struct {
			const char *input, *output;
		} asset_path;
		int intval;
	};
};

// Callback execution (1 <---> 1 link)
class GameEventHandler {
public:
	virtual ~GameEventHandler();

	void setEventTarget(GameEventHandler *eh);
	bool sendNewEvent(GameEvent &e); // for memory cleanup

protected:
	// Callback function to overload. Do NOT call manually!
	virtual bool OnEvent(GameEvent &e) = 0;

private:
	GameEventHandler *m_target = nullptr;
};
