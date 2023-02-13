#pragma once

#include "core/player.h"

class LocalPlayer : public Player {
public:
	LocalPlayer(peer_t peer_id) :
		Player(peer_id) {}

	static s32 gui_smiley_counter;
	s32 getGUISmileyId();

private:
	s32 m_gui_smiley_id = -1; // tag is + 1
};
