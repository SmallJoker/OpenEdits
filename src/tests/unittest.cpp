#include "unittest_internal.h"
#include <chrono>

void unittest_auth();
void unittest_chatcommand();
void unittest_connection();
void unittest_eeo_converter();
void unittest_database();
void unittest_packet();
void unittest_physics();
void unittest_utilities();
void unittest_world();

static auto time_start = std::chrono::steady_clock::now();
void unittest_tic()
{
	time_start = std::chrono::steady_clock::now();
}
void unittest_toc(const char *name)
{
	// Measure precise timings
	auto time_now = std::chrono::steady_clock::now();
	float dtime = std::chrono::duration<float>(time_now - time_start).count();
	printf("[%s] Timer took %.3f ms\n", name, dtime * 1000.0f);
}

//#define UNITTEST_CATCH_EX

void unittest()
{
	puts("==> Start unittest");

#ifdef UNITTEST_CATCH_EX
	try
#endif
	{
		unittest_auth();
		unittest_chatcommand();
		unittest_utilities();
		unittest_world();
		unittest_eeo_converter(); // depends on world
		unittest_physics(); // depends on world
		unittest_packet();
		unittest_database(); // depends on world & packet
		unittest_connection(); // depends on packet

		puts("<== Unittest completed");
	}
#ifdef UNITTEST_CATCH_EX
	catch (std::exception &e) {
		printf("=!= %s\n", e.what());
	}
#endif
}

