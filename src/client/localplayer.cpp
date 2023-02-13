#include "localplayer.h"

s32 LocalPlayer::gui_smiley_counter = -9999;

s32 LocalPlayer::getGUISmileyId()
{
	if (m_gui_smiley_id < 0) {
		m_gui_smiley_id = gui_smiley_counter;
		gui_smiley_counter += 3; // Face + nametag + effect
		ASSERT_FORCED(gui_smiley_counter >= 0, "Counting failed")
	}
	return m_gui_smiley_id;
}

