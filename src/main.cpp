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

const char *VERSION_STRING = "OpenEdits v1.0.6-dev"
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


int main(int argc, char *argv[])
{
	atexit(exit_cleanup);
	srand(time(nullptr));

	g_blockmanager = new BlockManager();
	g_blockmanager->doPackRegistration();

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
