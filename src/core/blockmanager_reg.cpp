#include "blockmanager.h"
#include "player.h"
#include "world.h"


// -------------- Block registrations -------------


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

static BP_STEP_CALLBACK(step_portal)
{
	player.acc.Y = Player::GRAVITY_NORMAL;

	if (pos == player.last_pos) {
		return; // No teleport loop
	}

	auto world = player.getWorld();
	BlockParams src_bp;
	if (!world->getParams(pos, &src_bp))
		return;

	auto positions = world->getBlocks(Block::ID_TELEPORTER, [&] (const Block &b) {
		blockpos_t dst_pos = world->getBlockPos(&b);
		if (dst_pos == pos)
			return false;

		BlockParams dst_bp;
		if (!world->getParams(dst_pos, &dst_bp))
			return false;

		return dst_bp.teleporter.id == src_bp.teleporter.dst_id;
	});

	if (positions.empty())
		return;

	auto dst_pos = positions[rand() % positions.size()];
	BlockParams dst_bp;
	world->getParams(dst_pos, &dst_bp);

	// Teleport!
	player.pos = core::vector2df(dst_pos.X, dst_pos.Y);
	int rotation = (dst_bp.teleporter.rotation - src_bp.teleporter.rotation + 4) % 4;

	switch (rotation) {
		case 1: // 90° CW
			player.vel = core::vector2df(
				- player.vel.Y,
				+ player.vel.X
			);
			break;
		case 2: // 180° turn
			player.vel *= -1;
			break;
		case 3: // 90° CCW
			player.vel = core::vector2df(
				+ player.vel.Y,
				- player.vel.X
			);
			break;
		default:
			// Same rotation
			break;
	}
}

static BP_STEP_CALLBACK(step_freeze)
{
	player.controls_enabled = false;
	player.vel *= 0.2f;
}

static BP_COLLIDE_CALLBACK(onCollide_coindoor)
{
	BlockParams params;
	player.getWorld()->getParams(pos, &params);

	return player.coins >= params.param_u8
		? BlockProperties::CollisionType::None
		: BlockProperties::CollisionType::Position;
}

static BP_COLLIDE_CALLBACK(onCollide_coingate)
{
	BlockParams params;
	player.getWorld()->getParams(pos, &params);

	return player.coins < params.param_u8
		? BlockProperties::CollisionType::None
		: BlockProperties::CollisionType::Position;
}

static BP_COLLIDE_CALLBACK(onCollide_oneway)
{
	// depends on player::DISTANCE_STEP

	if (!is_x && player.vel.Y >= 0 && player.pos.Y + 0.55f < pos.Y)
		return BlockProperties::CollisionType::Position; // normal step-up

	// Sideway gate
	return (is_x && player.pos.Y == pos.Y && !player.getControls().jump)
		? BlockProperties::CollisionType::Position
		: BlockProperties::CollisionType::None;
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
		registerPack(pack);
	}

	{
		BlockPack *pack = new BlockPack("beta");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 37, 38, 39, 40, 41, 42 };
		registerPack(pack);
	}

	{
		BlockPack *pack = new BlockPack("doors");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = {
			Block::ID_DOOR_R, Block::ID_DOOR_G, Block::ID_DOOR_B,
			Block::ID_GATE_R, Block::ID_GATE_G, Block::ID_GATE_B
		};
		registerPack(pack);

		for (bid_t id : pack->block_ids) {
			auto props = m_props[id];
			if (id >= Block::ID_GATE_R) {
				props->setTiles({ BlockDrawType::Action, BlockDrawType::Solid });
			} else {
				props->setTiles({ BlockDrawType::Solid, BlockDrawType::Action });
			}
		}
	}

	{
		BlockPack *pack = new BlockPack("factory");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 45, 46, 47, 48, 49 };
		registerPack(pack);
	}

	{
		BlockPack *pack = new BlockPack("candy");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 60, 61, 62, 63, 64, 65, 66, 67 };
		registerPack(pack);
		// one-way gates
		for (size_t i = 61; i <= 64; ++i)
			m_props[i]->onCollide = onCollide_oneway;
		for (bid_t id : pack->block_ids)
			m_props[id]->tiles[0].have_alpha = true;
	}

	{
		BlockPack *pack = new BlockPack("action");
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
		BlockPack *pack = new BlockPack("keys");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_KEY_R, Block::ID_KEY_G, Block::ID_KEY_B };
		registerPack(pack);

		for (bid_t id : pack->block_ids)
			m_props[id]->trigger_on_touch = true;
	}

	// For testing. bouncy blue basic block
	m_props[10]->onCollide = onCollide_b10_bouncy;

	{
		// Spawn block only (for now)
		BlockPack *pack = new BlockPack("spike");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_CHECKPOINT, Block::ID_SPIKES };
		registerPack(pack);

		auto props = m_props[Block::ID_CHECKPOINT];
		props->trigger_on_touch = true;
		props->setTiles({ BlockDrawType::Decoration, BlockDrawType::Action });

		props = m_props[Block::ID_SPIKES];
		props->paramtypes = BlockParams::Type::U8;
		props->trigger_on_touch = true;
		props->setTiles({
			BlockDrawType::Decoration, BlockDrawType::Decoration,
			BlockDrawType::Decoration, BlockDrawType::Decoration
		});
		m_props[Block::ID_SPIKES]->step = step_freeze;
	}

	{
		// Spawn block only (for now)
		BlockPack *pack = new BlockPack("owner");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_SPAWN, Block::ID_SECRET };
		registerPack(pack);

		auto props = m_props[Block::ID_SECRET];
		props->trigger_on_touch = true;
		props->setTiles({ BlockDrawType::Solid, BlockDrawType::Solid });
		props->tiles[0].have_alpha = true;
	}

	{
		BlockPack *pack = new BlockPack("coins");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_COIN, Block::ID_COINDOOR, Block::ID_COINGATE };
		registerPack(pack);

		auto props = m_props[Block::ID_COIN];
		props->trigger_on_touch = true;
		props->setTiles({ BlockDrawType::Decoration, BlockDrawType::Action });

		props = m_props[Block::ID_COINDOOR];
		props->paramtypes = BlockParams::Type::U8;
		props->setTiles({ BlockDrawType::Solid, BlockDrawType::Solid });
		// Walk-through is player-specific, hence using the onCollide callback
		props->tiles[1].have_alpha = true;
		props->onCollide = onCollide_coindoor;

		props = m_props[Block::ID_COINGATE];
		props->paramtypes = BlockParams::Type::U8;
		props->setTiles({ BlockDrawType::Solid, BlockDrawType::Solid });
		// Walk-through is player-specific, hence using the onCollide callback
		props->tiles[0].have_alpha = true;
		props->onCollide = onCollide_coingate;
	}

	{
		BlockPack *pack = new BlockPack("teleporter");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_TELEPORTER };
		registerPack(pack);

		auto props = m_props[Block::ID_TELEPORTER];
		props->paramtypes = BlockParams::Type::Teleporter;
		props->trigger_on_touch = true;
		props->setTiles({
			BlockDrawType::Action, BlockDrawType::Action,
			BlockDrawType::Action, BlockDrawType::Action
		});
		props->step = step_portal;
	}

	// Decoration
	{
		BlockPack *pack = new BlockPack("spring");
		pack->default_type = BlockDrawType::Decoration;
		pack->block_ids = { 233, 234, 235, 236, 237, 238, 239, 240 };
		registerPack(pack);
	}

	// Backgrounds
	{
		// "basic" or "dark"
		BlockPack *pack = new BlockPack("simple");
		pack->default_type = BlockDrawType::Background;
		pack->block_ids = { 500, 501, 502, 503, 504, 505, 506 };
		registerPack(pack);
	}
}

