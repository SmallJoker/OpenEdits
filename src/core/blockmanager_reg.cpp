#include "blockmanager.h"
#include "player.h"

static BP_STEP_CALLBACK(step_arrow_left)
{
	player.acc.X = -Player::GRAVITY_NORMAL;
}

static BP_STEP_CALLBACK(step_arrow_up)
{
	player.acc.Y = -Player::GRAVITY_NORMAL;
}

static BP_STEP_CALLBACK(step_arrow_right)
{
	player.acc.X = +Player::GRAVITY_NORMAL;
}

static BP_STEP_CALLBACK(step_arrow_none)
{
}

void BlockManager::registerUnittestPacks()
{
	BlockPack *pack;

	{
		pack = new BlockPack("action");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { 0, 1, 2, 3, 4 };
		registerPack(pack);

		m_props[0]->color = 0xFF000000;
		m_props[1]->step = step_arrow_left;
		m_props[2]->step = step_arrow_up;
		m_props[3]->step = step_arrow_right;
		m_props[4]->step = step_arrow_none;
		m_props[4]->viscosity = 0.25f;
	}

	{
		pack = new BlockPack("basic");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 9, 10, 11, 12, 13, 14, 15 };
		registerPack(pack);
	}

	{
		pack = new BlockPack("teleporter");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_TELEPORTER };
		registerPack(pack);

		auto props = getPropsForModification(Block::ID_TELEPORTER);
		props->paramtypes = BlockParams::Type::Teleporter;
		props->setTiles({
			BlockDrawType::Action, BlockDrawType::Action,
			BlockDrawType::Action, BlockDrawType::Action
		});
	}

	{
		pack = new BlockPack("simple");
		pack->default_type = BlockDrawType::Background;
		pack->block_ids = { 500, 501, 502, 503, 504, 505, 506 };
		registerPack(pack);
	}

	sanityCheck();
}
