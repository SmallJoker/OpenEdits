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

const char *VERSION_STRING = "OpenEdits v1.0.3-dev"
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



static void step_arrow_left(float dtime, Player &c, blockpos_t pos)
{
	c.acc.X -= Player::GRAVITY_NORMAL;
}

static void step_arrow_up(float dtime, Player &c, blockpos_t pos)
{
	c.acc.Y -= Player::GRAVITY_NORMAL;
}

static void step_arrow_right(float dtime, Player &c, blockpos_t pos)
{
	c.acc.X += Player::GRAVITY_NORMAL;
}

static void step_arrow_none(float dtime, Player &c, blockpos_t pos)
{
}

static bool onCollide_b10_bouncy(float dtime, Player &c, const core::vector2d<s8> dir)
{
	if (dir.X) {
		c.vel.X *= -0.4f;
	} else if (dir.Y) {
		c.vel.Y *= -1.5f;
	}
	return false;
}

static void register_packs()
{
	{
		BlockPack *pack = new BlockPack("action");
		pack->default_type = BlockDrawType::Action;
		pack->block_ids = { 0, 1, 2, 3, 4 };
		g_blockmanager->registerPack(pack);
	}

	{
		BlockPack *pack = new BlockPack("basic");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 9, 10, 11, 12, 13, 14, 15 };
		g_blockmanager->registerPack(pack);
	}

	{
		BlockPack *pack = new BlockPack("factory");
		pack->default_type = BlockDrawType::Solid;
		pack->block_ids = { 45, 46, 47, 48, 49 };
		g_blockmanager->registerPack(pack);
	}

	g_blockmanager->getProps(1)->step = step_arrow_left;
	g_blockmanager->getProps(2)->step = step_arrow_up;
	g_blockmanager->getProps(3)->step = step_arrow_right;
	g_blockmanager->getProps(4)->step = step_arrow_none;
	g_blockmanager->getProps(10)->onCollide = onCollide_b10_bouncy; // blue

	// Backgrounds
	{
		BlockPack *pack = new BlockPack("simple");
		pack->default_type = BlockDrawType::Background;
		pack->block_ids = { 500, 501, 502, 503, 504, 505, 506 };
		g_blockmanager->registerPack(pack);
	}
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
