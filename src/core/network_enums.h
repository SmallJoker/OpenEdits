#pragma once

#include <stdint.h>

// In sync with client_packethandler.cpp
enum class Packet2Client : uint16_t {
	Quack = 0,
	Hello,
	Message,
	Auth,
	Lobby,
	WorldData, // initialization or reset
	Join,
	Leave,
	SetPosition, // generally respawn
	Move,
	Chat,
	PlaceBlock,
	Key, // key blocks
	GodMode,
	Smiley,
	PlayerFlags,
	WorldMeta,
	ChatReplay,
	MediaList,
	MediaReceive,
	placeholder,
	MAX_END
};

// In sync with server_packethandler.cpp
enum class Packet2Server : uint16_t {
	Quack = 0,
	Hello,
	Auth,
	GetLobby,
	Join,
	Leave,
	Move,
	Chat,
	PlaceBlock,
	OnTouchBlock, // key/kill ?
	GodMode,
	Smiley,
	FriendAction,
	MediaRequest,
	MAX_END
};
