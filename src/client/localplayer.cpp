#include "localplayer.h"
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

bool LocalPlayer::updateCoinCount()
{
	int old_coins = coins;

	auto collected = m_world->getBlocks(Block::ID_COIN, [](Block &b) -> bool {
		return b.param1 > 0;
	});

	coins = std::min<size_t>(127, collected.size());

	if (coins == old_coins)
		return false;

	int my_coins = coins; // move to stack
	for (Block *b = m_world->begin(); b != m_world->end(); ++b) {
		switch (b->id) {
			case Block::ID_COINDOOR:
			case Block::ID_COINGATE:
			{

				u8 howmany = b->param1 & ~Block::P1_FLAG_TILE1;
				if (my_coins >= howmany)
					b->param1 |= Block::P1_FLAG_TILE1;
				else
					b->param1 = howmany;
			}
			break;
		}

	}

	return true;
}

