#pragma once

#include "core/auth.h"
#include "core/player.h"
#include "core/timer.h"

enum class RemotePlayerState {
	Invalid,
	Login,
	Idle,
	WorldJoin,
	WorldPlay
};

class RemotePlayer : public Player {
public:
	RemotePlayer(peer_t peer_id, uint16_t protocol_version);

	const uint16_t protocol_version;

	Auth auth;
	RemotePlayerState state = RemotePlayerState::Invalid;

	// Rate limits
	RateLimit rl_blocks;
	RateLimit rl_chat;

	void runAnticheat(float dtime);
	// TODO: Reset when joining a world
	float time_since_move_pkt = 0;
	float cheat_probability = -1;
};
