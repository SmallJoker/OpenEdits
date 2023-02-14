#include "unittest_internal.h"

void unittest_connection();
void unittest_database();
void unittest_packet();
void unittest_utilities();
void unittest_world();

void unittest()
{
	puts("==> Start unittest");
	unittest_database();
	exit(0);
	unittest_utilities();
	unittest_world();
	unittest_packet();
	unittest_connection();
	puts("<== Unittest completed");
}

