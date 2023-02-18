#include "core/blockmanager.h"
#include "core/player.h"
#include "gui/gui.h"
#include <string.h> // strcmp

#include <chrono>
#include <thread>
void sleep_ms(long delay)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

const char *VERSION_STRING = "OpenEdits v1.0.4-dev"
#ifdef NDEBUG
	" (Release)"
#else
	" (Debug)"
#endif
	;


void unittest();

static void exit_cleanup()
{
	delete g_blockmanager;
}



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

static BP_STEP_CALLBACK(step_key)
{
	if (player.triggered_blocks)
		player.triggered_blocks->emplace(pos);
}

static BP_COLLIDE_CALLBACK(onCollide_b10_bouncy)
{
	if (dir.X) {
		player.vel.X *= -0.4f;
	} else if (dir.Y) {
		player.vel.Y *= -1.5f;
	}
	return false;
}

static void register_packs()
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
		pack->block_ids = { Block::ID_DOOR_R, Block::ID_DOOR_G, Block::ID_DOOR_B };
		g_blockmanager->registerPack(pack);

		for (int i = Block::ID_DOOR_R; i <= Block::ID_DOOR_B; ++i) {
			auto props = g_blockmanager->getProps(i);
			props->condition = BlockTileCondition::NotZero;
			props->tiles[1].type = BlockDrawType::Action;
			props->tiles[1].texture_offset = 3;
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

		g_blockmanager->getProps(1)->step = step_arrow_left;
		g_blockmanager->getProps(2)->step = step_arrow_up;
		g_blockmanager->getProps(3)->step = step_arrow_right;
		g_blockmanager->getProps(4)->step = step_arrow_none;
	}

	{
		BlockPack *pack = new BlockPack("keys");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { 6, 7, 8 };
		g_blockmanager->registerPack(pack);

		g_blockmanager->getProps(6)->step = step_key;
		g_blockmanager->getProps(7)->step = step_key;
		g_blockmanager->getProps(8)->step = step_key;
	}

	// For testing. bouncy blue basic block
	g_blockmanager->getProps(10)->onCollide = onCollide_b10_bouncy;

	{
		// Spawn block only (for now)
		BlockPack *pack = new BlockPack("owner");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { Block::ID_SPAWN };
		g_blockmanager->registerPack(pack);
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

/*
	Key RGB: 6
	Door RGB: 23
	Gate RGB: 26
	Coin: 100
	Coin door: 43
	Spawn: 255
	Secret (invisible): 50
*/
}

int main(int argc, char *argv[])
{
	atexit(exit_cleanup);
	srand(time(nullptr));

	g_blockmanager = new BlockManager();
	register_packs();

	if (argc >= 2) {
		if (strcmp(argv[1], "--version") == 0) {
			puts(VERSION_STRING);
			return EXIT_SUCCESS;
		}
		if (strcmp(argv[1], "--unittest") == 0) {
			// Depends on BlockManager and ENet
			unittest();
			return EXIT_SUCCESS;
		}
		puts("-!- Unknown command line option.");
		return EXIT_FAILURE;
	}

	Gui gui;
	gui.run();

	return EXIT_SUCCESS;
}
