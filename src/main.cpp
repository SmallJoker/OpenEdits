#include "core/blockmanager.h"
#if BUILD_CLIENT
	#include "gui/gui.h"
	static Gui *my_gui = nullptr;
#endif
#include "server/server.h"
#include <string.h> // strcmp
#include "version.h"

#ifdef __unix__
	#include <signal.h>
#endif

#include <chrono>
#include <thread>
void sleep_ms(long delay)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}


void unittest();

static bool shutdown_requested = false;
static void exit_cleanup()
{
	shutdown_requested = true;

#if BUILD_CLIENT
	if (my_gui)
		my_gui->requestShutdown();
#endif
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

#if BUILD_CLIENT
	// This is the instance used by the client and GUI
	g_blockmanager = new BlockManager();
	g_blockmanager->doPackRegistration();
#endif

	bool run_server = (BUILD_CLIENT == 0);
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
		if (strcmp(argv[1], "--server") == 0) {
			// Dedicated server
			run_server = true;
			goto run;
		}
		puts("-!- Unknown command line option.");
		return EXIT_FAILURE;
	}

run:
#ifdef __unix__
	{
		struct sigaction act;
		act.sa_handler = sigint_handler;
		sigaction(SIGINT, &act, NULL);
		sigaction(SIGTERM, &act, NULL);
	}
#endif

	if (run_server) {
		Server server;
		auto t_last = std::chrono::steady_clock::now();
		while (!shutdown_requested) {
			float dtime;
			{
				// Measure precise timings
				auto t_now = std::chrono::steady_clock::now();
				dtime = std::chrono::duration<float>(t_now - t_last).count();
				t_last = t_now;
			}

			server.step(dtime);
			sleep_ms(100);
		}

		return EXIT_SUCCESS;
	}

#if BUILD_CLIENT
	Gui gui;
	my_gui = &gui;
	gui.run();

	delete g_blockmanager;
#endif

	return EXIT_SUCCESS;
}
