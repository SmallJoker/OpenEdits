#pragma once

#include "core/macros.h"
#include "core/player.h"

class RemotePlayer : public Player {
public:
	RemotePlayer(peer_t peer_id);

	const peer_t peer_id;

	void *world_id;
private:
};
