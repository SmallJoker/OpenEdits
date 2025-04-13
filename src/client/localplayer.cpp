#include "localplayer.h"
#include "core/blockmanager.h"
#include "core/script/script.h"
#include "core/world.h"

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

bool LocalPlayer::updateCoinCount(bool force)
{
	int old_coins = coins;

	auto collected = m_world->getBlocks(Block::ID_COIN, [](Block &b) -> bool {
		return b.tile > 0;
	});

	coins = std::min<size_t>(127, collected.size());

	if (coins == old_coins && !force)
		return false;

	if (m_script && !m_script->getBlockMgr()->isHardcoded())
		return false; // to be done by script

	int my_coins = coins; // move to stack
	for (Block *b = m_world->begin(); b != m_world->end(); ++b) {
		switch (b->id) {
			case Block::ID_COINDOOR:
			case Block::ID_COINGATE:
			{
				BlockParams params;
				m_world->getParams(m_world->getBlockPos(b), &params);
				if (my_coins >= params.param_u8)
					b->tile = 1;
				else
					b->tile = 0;
			}
			break;
		}

	}

	return true;
}

