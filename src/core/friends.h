#pragma once

#include <string>
#include <cstdint>


struct LobbyFriend {
	// Fixed by protocol and database
	enum class Type : uint8_t {
		None = 0,
		// Main database values (static):
		Accepted,
		Pending,
		Rejected,
		// For client use (dynamic):
		PendingIncoming,
		FriendOffline,
		FriendOnline,
		MAX_INVALID
	} type;

	std::string name;
	std::string world_id; // if online
};
