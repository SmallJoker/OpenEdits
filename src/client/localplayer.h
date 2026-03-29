#pragma once

#include "core/player.h"

class LocalPlayer : public Player {
public:
	LocalPlayer(peer_t peer_id) :
		Player(peer_id) {}

	int smiley_id = 0;

	s32 node_id = -1; //< ID of the ISceneNode of this player
	float speech_countdown = -1; //< Speech bubble countdown

	// Executa only on the own player!
	void updateCoinCount(bool force);

	blockpos_t last_sent_pos;
};
