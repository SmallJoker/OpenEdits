#include "unittest_internal.h"

void unittest_chatcommand();
void unittest_connection();
void unittest_database();
void unittest_packet();
void unittest_utilities();
void unittest_world();

void unittest()
{
	puts("==> Start unittest");
	unittest_chatcommand();
	unittest_utilities();
	unittest_world();
	unittest_packet();
	unittest_database(); // depends on world & packet
	unittest_connection(); // depends on packet
	puts("<== Unittest completed");
}

