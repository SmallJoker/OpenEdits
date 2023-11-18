#pragma once

#include <string>
#include <cstdint>


struct LobbyFriend {
	// Fixed by protocol
	enum class Type : uint8_t {
		None = 0,
		Rejected,
		RequestOutgoing, // waiting other player
		RequestIncoming, // waiting current player
		FriendOffline,
		FriendOnline,
		MAX_INVALID
	} type;

	std::string name;
	std::string world_id; // if online
};
