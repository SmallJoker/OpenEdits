#pragma once

#include "core/macros.h"
#include "core/player.h"

class LocalPlayer : public Player {
public:
	LocalPlayer(peer_t peer_id);

	const peer_t peer_id;
;
private:
};
