#include <iostream>
#include "core/blockmanager.h"
#include "core/connection.h"
#include "core/packet.h"
#include "gui/gui.h"
#include <string.h>

#include <chrono>
#include <thread>
void sleep_ms(long delay)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

void unittest();

static void exit_cleanup()
{
	delete g_blockmanager;
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

}

int main(int argc, char *argv[])
{
	atexit(exit_cleanup);
	srand(time(nullptr));

	g_blockmanager = new BlockManager();
	register_packs();

	if (argc >= 2 && strcmp(argv[1], "--unittest") == 0) {
		unittest();
		return EXIT_SUCCESS;
	}

	Gui gui;
	gui.run();

	return EXIT_SUCCESS;
}
