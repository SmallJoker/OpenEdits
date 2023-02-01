#pragma once

#include "core/macros.h"
#include "core/player.h"
#include "core/environment.h"

class RemotePlayer : public Player {
public:
	RemotePlayer(peer_t peer_id, uint16_t protocol_version);

	const peer_t peer_id;
	const uint16_t protocol_version;

	worldid_t world_id = 0;
private:
};
