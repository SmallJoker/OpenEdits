#pragma once

#include <cstdint>
#include <string>

class Player;

struct GameEvent {
	enum {
		GE_DIALOG_MESSAGE,
		GE_PLAYER_JOIN,
		GE_PLAYER_LEAVE,
		GE_PLAYER_SAY,
		GE_MAP_ADD_BLOCK,
		GE_MAP_REMOVE_BLOCK
	} event;

	Player *player;
	std::string str;
	int64_t i64_3[3];
};

class GameEventHandler {
public:
	virtual bool OnEvent(const GameEvent &e) = 0;
};
