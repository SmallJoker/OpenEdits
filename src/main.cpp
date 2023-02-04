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
	BlockPack *pack = new BlockPack("action");
	pack->default_type = BlockDrawType::Action;
	pack->block_ids = { 0, 1, 2, 3 };
	g_blockmanager->registerPack(pack);
}

int main(int argc, char *argv[])
{
	if (argc >= 2 && strcmp(argv[1], "--unittest") == 0) {
		unittest();
		return EXIT_SUCCESS;
	}

	atexit(exit_cleanup);

	g_blockmanager = new BlockManager();
	register_packs();

	Gui gui;
	gui.run();

	return EXIT_SUCCESS;
}
