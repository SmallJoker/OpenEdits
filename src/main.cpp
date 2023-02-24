#include "core/blockmanager.h"
#include "gui/gui.h"
#include <string.h> // strcmp

#ifdef __unix__
	#include <signal.h>
#endif

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

static Gui *my_gui = nullptr;
static void exit_cleanup()
{
	if (my_gui)
		my_gui->requestShutdown();
}

static void sigint_handler(int signal)
{
	printf("\nmain: Received signal %i\n", signal);
	exit_cleanup();
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

#ifdef __unix__
	{
		struct sigaction act;
		act.sa_handler = sigint_handler;
		sigaction(SIGINT, &act, NULL);
		sigaction(SIGTERM, &act, NULL);
	}
#endif

	Gui gui;
	my_gui = &gui;
	gui.run();

	delete g_blockmanager;

	return EXIT_SUCCESS;
}
