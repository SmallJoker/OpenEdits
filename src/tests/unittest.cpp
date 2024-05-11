#include "unittest_internal.h"
#include <chrono>

void unittest_auth();
void unittest_chatcommand();
void unittest_connection();
void unittest_eeo_converter();
void unittest_database();
void unittest_mediamanager();
void unittest_packet();
void unittest_physics();
void unittest_script();
void unittest_sound();
void unittest_utilities();
void unittest_world();
void unittest_gui_layout(int which);
void unittest_gui_gameplay();

static auto time_start = std::chrono::steady_clock::now();
void unittest_tic()
{
	time_start = std::chrono::steady_clock::now();
}
void unittest_toc(const char *name)
{
	// Measure precise timings
	auto time_now = std::chrono::steady_clock::now();
	double dtime = std::chrono::duration<double>(time_now - time_start).count();
	if (dtime > 2E-3) {
		printf("[%s] Timer took %.3f ms\n", name, dtime * 1E3);
	} else {
		printf("[%s] Timer took %.3f us\n", name, dtime * 1E6);
	}
}

//#define UNITTEST_CATCH_EX

void unittest()
{
	puts("==> Start unittest");

#ifdef UNITTEST_CATCH_EX
	try
#endif
	{
		unittest_mediamanager();
		unittest_script();
		//unittest_gui_layout(3);

		unittest_auth();
		unittest_chatcommand();
		unittest_utilities();
		unittest_packet();
		unittest_world(); // depends on packet
		unittest_eeo_converter(); // depends on world
		unittest_physics(); // depends on world
		unittest_database(); // depends on world & packet
		unittest_connection(); // depends on packet
		unittest_script(); // depends on world & physics (player)
		unittest_sound();
		unittest_gui_gameplay();

		puts("<== Unittest completed");
	}
#ifdef UNITTEST_CATCH_EX
	catch (std::exception &e) {
		printf("=!= %s\n", e.what());
	}
#endif
}

