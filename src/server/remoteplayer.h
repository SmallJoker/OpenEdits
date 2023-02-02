#pragma once

#include "core/macros.h"
#include "core/player.h"
#include "core/environment.h"

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

	const peer_t peer_id;
	const uint16_t protocol_version;

	RemotePlayerState state = RemotePlayerState::Invalid;

	worldid_t world_id = 0;
private:
};
