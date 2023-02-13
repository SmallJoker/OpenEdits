#pragma once

#include "core/world.h"
#include <cstdint>
#include <string>

using namespace irr;

class Player;

/*
	Holds callback data of various kinds for stuff that is unique
	to the Client or GUI. I want them to be decoupled, hence this code.
*/
struct GameEvent {
	// GUI --> Client
	enum G2C_Enum {
		G2C_INVALID,
		G2C_LOBBY_REQUEST,
		G2C_JOIN,
		G2C_LEAVE,
		G2C_CHAT,
	} type_g2c = G2C_INVALID;

	// Client --> GUI
	enum C2G_Enum {
		C2G_INVALID,
		C2G_DIALOG,
		C2G_DISCONNECT,
		C2G_LOBBY_UPDATE,
		C2G_JOIN,
		C2G_LEAVE,
		C2G_MAP_UPDATE,
		C2G_PLAYER_JOIN,
		C2G_PLAYER_LEAVE,
		C2G_PLAYER_CHAT
	} type_c2g = C2G_INVALID;

	GameEvent(G2C_Enum v) : type_g2c(v) {}
	GameEvent(C2G_Enum v) : type_c2g(v) {}

	struct PlayerChat {
		Player *player;
		std::string message;
	};

	struct BlockData {
		blockpos_t pos;
		Block b;
	};

	union {
		std::string *text;
		Player *player;
		PlayerChat *player_chat;
		BlockData *block;
	};
};

// Callback execution (1 <---> 1 link)
class GameEventHandler {
public:
	virtual ~GameEventHandler();

	void setEventHandler(GameEventHandler *eh);
	bool sendNewEvent(GameEvent &e); // for memory cleanup

	// Callback function to overload. Do NOT call manually!
	virtual bool OnEvent(GameEvent &e) = 0;

private:
	GameEventHandler *m_eventhandler = nullptr;
};
