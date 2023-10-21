#include "core/blockmanager.h"
#if BUILD_CLIENT
	#include "gui/gui.h"
	static Gui *my_gui = nullptr;
#endif
#include "server/database_auth.h" // AuthAccount
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

extern BlockManager *g_blockmanager;

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

static int run_client()
{
	if (!BUILD_CLIENT) {
		puts("-!- Client is not available on this build.");
		return EXIT_FAILURE;
	}

#if BUILD_CLIENT
	Gui gui;
	my_gui = &gui;
	gui.run();
	my_gui = nullptr;
#endif

	return EXIT_SUCCESS;
}

static int run_server()
{
	Server server(&shutdown_requested);
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

static int server_setrole(char *username, char *role)
{
	AuthAccount::AccountLevel newlevel = AuthAccount::AL_INVALID;

	if (strcmp(role, "normal") == 0) {
		newlevel = AuthAccount::AL_REGISTERED;
	} else if (strcmp(role, "moderator") == 0) {
		newlevel = AuthAccount::AL_MODERATOR;
	} else if (strcmp(role, "admin") == 0) {
		newlevel = AuthAccount::AL_SERVER_ADMIN;
	}

	if (newlevel == AuthAccount::AL_INVALID) {
		puts("-!- Unknown role. Available: normal, moderator, admin");
		return EXIT_FAILURE;
	}

	char *ptr = username;
	while (*ptr) {
		*ptr = toupper(*ptr);
		ptr++;
	}

	DatabaseAuth auth_db;
	if (!auth_db.tryOpen("server_auth.sqlite"))
		return EXIT_FAILURE; // logged by Database::ok

	AuthAccount acc;
	if (!auth_db.load(username, &acc)) {
		puts("-!- This account is yet not registered.");
		return EXIT_FAILURE;
	}

	auth_db.enableWAL();
	acc.level = newlevel;
	if (auth_db.save(acc)) {
		puts("--- Role changed successfully!");
		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE; // logged by Database::ok
}

static int parse_args(int argc, char *argv[])
{
	if (argc < 2)
		return BUILD_CLIENT ? run_client() : run_server();

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
		return run_server();
	}
	if (strcmp(argv[1], "--setrole") == 0) {
		if (argc != 4) {
			puts("-!- Missing arguments: --setrole USERNAME ROLE");
			return EXIT_FAILURE;
		}
		return server_setrole(argv[2], argv[3]);
	}

	puts("-!- Unknown command line option.");
	return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
	atexit(exit_cleanup);
	srand(time(nullptr));

#ifdef __unix__
	struct sigaction act;
	memset(&act, 1, sizeof(act));
	act.sa_handler = sigint_handler;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
#endif

	// Used by Gui, Client and Unittests but not Server.
	g_blockmanager = new BlockManager();
	g_blockmanager->doPackRegistration();

	int status = parse_args(argc, argv);

	delete g_blockmanager;

	return status;
}
