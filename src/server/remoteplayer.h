#pragma once

#include "core/auth.h"
#include "core/player.h"
#include "core/timer.h"
#include <unordered_set>

enum class Packet2Client : uint16_t;

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

	Packet createPacket(Packet2Client type) const;

	const uint16_t protocol_version;

	Auth auth;
	RemotePlayerState state = RemotePlayerState::Invalid;

	struct {
		bool needs_audiovisuals = true;
		u32 total_sent = 0; // sort of a rate limit
		std::unordered_set<std::string> requested;
	} media;

	// Rate limits (incoing requests)
	RateLimit rl_blocks;
	RateLimit rl_chat;
	RateLimit rl_scriptevents;

	void runAnticheat(float dtime);
	// TODO: Reset when joining a world
	float time_since_move_pkt = 0;
	float cheat_probability = -1;
};
