#include "localplayer.h"
#include "core/blockmanager.h"
#include "core/script/script.h"
#include "core/world.h"

void LocalPlayer::updateCoinCount(bool force)
{
	if (m_script && !m_script->getBlockMgr()->isHardcoded())
		return; // done by script

	int old_coins = coins;

	auto collected = m_world->getBlocks(Block::ID_COIN, [](Block &b) -> bool {
		return b.tile > 0;
	});

	coins = std::min<size_t>(127, collected.size());

	if (coins == old_coins && !force)
		return;

	int my_coins = coins; // move to stack
	auto rect = m_world->modified_rect;
	for (Block *b = m_world->begin(); b != m_world->end(); ++b) {
		switch (b->id) {
			case Block::ID_COINDOOR:
			case Block::ID_COINGATE:
			{
				blockpos_t bp = m_world->getBlockPos(b);
				BlockParams params;
				m_world->getParams(bp, &params);
				if (my_coins >= params.param_u8)
					b->tile = 1;
				else
					b->tile = 0;
				rect.addInternalPoint(bp);
			}
			break;
		}
	}
	m_world->modified_rect = rect;
}

