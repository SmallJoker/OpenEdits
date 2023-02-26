#include "blockmanager.h"
#include "player.h"
#include "world.h"


// -------------- Block registrations -------------


static BP_STEP_CALLBACK(step_arrow_left)
{
	player.acc.X -= Player::GRAVITY_NORMAL;
}

static BP_STEP_CALLBACK(step_arrow_up)
{
	player.acc.Y -= Player::GRAVITY_NORMAL;
}

static BP_STEP_CALLBACK(step_arrow_right)
{
	player.acc.X += Player::GRAVITY_NORMAL;
}

static BP_STEP_CALLBACK(step_arrow_none)
{
}

static BP_COLLIDE_CALLBACK(onCollide_coindoor)
{
	BlockParams params;
	player.getWorld()->getParams(pos, &params);

	return player.coins >= params.gate.value
		? BlockProperties::CollisionType::None
		: BlockProperties::CollisionType::Position;
}

static BP_COLLIDE_CALLBACK(onCollide_coingate)
{
	BlockParams params;
	player.getWorld()->getParams(pos, &params);

	return player.coins < params.gate.value
		? BlockProperties::CollisionType::None
		: BlockProperties::CollisionType::Position;
}

static BP_COLLIDE_CALLBACK(onCollide_b10_bouncy)
{
	if (is_x) {
		player.pos.X = std::roundf(player.pos.X);
		player.vel.X *= -0.4f;
	} else {
		player.pos.Y = std::roundf(player.pos.Y);
		player.vel.Y *= -1.5f;
	}
	return BlockProperties::CollisionType::None;
}

void BlockManager::doPackRegistration()
{
	{
		BlockPack *pack = new BlockPack("basic");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 9, 10, 11, 12, 13, 14, 15 };
		g_blockmanager->registerPack(pack);
	}

	{
		BlockPack *pack = new BlockPack("doors");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = {
			Block::ID_DOOR_R, Block::ID_DOOR_G, Block::ID_DOOR_B,
			Block::ID_GATE_R, Block::ID_GATE_G, Block::ID_GATE_B
		};
		g_blockmanager->registerPack(pack);

		for (bid_t id : pack->block_ids) {
			auto props = m_props[id];
			if (id >= Block::ID_GATE_R) {
				props->tiles[0].type = BlockDrawType::Action;
				props->tiles[1].type = BlockDrawType::Solid;
			} else {
				props->tiles[1].type = BlockDrawType::Action;
			}
		}
	}

	{
		BlockPack *pack = new BlockPack("factory");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 45, 46, 47, 48, 49 };
		g_blockmanager->registerPack(pack);
	}

	{
		BlockPack *pack = new BlockPack("action");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { 0, 1, 2, 3, 4 };
		g_blockmanager->registerPack(pack);

		m_props[1]->step = step_arrow_left;
		m_props[2]->step = step_arrow_up;
		m_props[3]->step = step_arrow_right;
		m_props[4]->step = step_arrow_none;
	}

	{
		BlockPack *pack = new BlockPack("keys");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_KEY_R, Block::ID_KEY_G, Block::ID_KEY_B };
		g_blockmanager->registerPack(pack);

		for (bid_t id : pack->block_ids)
			m_props[id]->trigger_on_touch = true;
	}

	// For testing. bouncy blue basic block
	m_props[10]->onCollide = onCollide_b10_bouncy;

	{
		// Spawn block only (for now)
		BlockPack *pack = new BlockPack("owner");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_SPAWN, Block::ID_SECRET };
		g_blockmanager->registerPack(pack);

		auto props = m_props[Block::ID_SECRET];
		props->trigger_on_touch = true;
		props->tiles[0].type = BlockDrawType::Solid;
		props->tiles[0].have_alpha = true;
		props->tiles[1].type = BlockDrawType::Solid;
	}

	{
		BlockPack *pack = new BlockPack("coins");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_COIN, Block::ID_COINDOOR, Block::ID_COINGATE };
		g_blockmanager->registerPack(pack);

		auto props = m_props[Block::ID_COIN];
		props->trigger_on_touch = true;
		props->tiles[0].type = BlockDrawType::Decoration; // draw above players
		props->tiles[1].type = BlockDrawType::Action;

		props = m_props[Block::ID_COINDOOR];
		props->paramtypes = BlockParams::Type::Gate;
		// Walk-through is player-specific, hence using the onCollide callback
		props->tiles[0].type = BlockDrawType::Solid;
		props->tiles[1].type = BlockDrawType::Solid;
		props->tiles[1].have_alpha = true;
		props->onCollide = onCollide_coindoor;

		props = m_props[Block::ID_COINGATE];
		props->paramtypes = BlockParams::Type::Gate;
		// Walk-through is player-specific, hence using the onCollide callback
		props->tiles[0].type = BlockDrawType::Solid;
		props->tiles[0].have_alpha = true;
		props->tiles[1].type = BlockDrawType::Solid;
		props->onCollide = onCollide_coingate;
	}

	// Decoration
	{
		BlockPack *pack = new BlockPack("spring");
		pack->default_type = BlockDrawType::Decoration;
		pack->block_ids = { 233, 234, 235, 236, 237, 238, 239, 240 };
		g_blockmanager->registerPack(pack);
	}

	// Backgrounds
	{
		// "basic" or "dark"
		BlockPack *pack = new BlockPack("simple");
		pack->default_type = BlockDrawType::Background;
		pack->block_ids = { 500, 501, 502, 503, 504, 505, 506 };
		g_blockmanager->registerPack(pack);
	}
}

