#pragma once

#include <stdint>

enum class EventType : uint16_t {
	PLAYER_JOIN,
	PLAYER_LEAVE,
	PLAYER_MOVE,
	BLOCK_ADD,
	BLOCK_REMOVE,
	CHAT_MESSAGE
};
