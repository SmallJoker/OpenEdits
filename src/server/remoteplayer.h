#pragma once

#include "core/player.h"

enum class RemotePlayerState {
	Invalid,
	Uninitialized,
	Idle,
	WorldJoin,
	WorldPlay
};

class RemotePlayer : public Player {
public:
	RemotePlayer(peer_t peer_id, uint16_t protocol_version);

	const uint16_t protocol_version;

	RemotePlayerState state = RemotePlayerState::Invalid;
};
