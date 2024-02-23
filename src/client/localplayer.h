#pragma once

#include "core/player.h"

class LocalPlayer : public Player {
public:
	LocalPlayer(peer_t peer_id) :
		Player(peer_id) {}

	static s32 gui_smiley_counter;
	s32 getGUISmileyId();

	int smiley_id = 0;

	// Executa only on the own player!
	bool updateCoinCount(bool force = false);

	blockpos_t last_sent_pos;

private:
	s32 m_gui_smiley_id = -1; // tag is + 1
};
