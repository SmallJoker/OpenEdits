#include "unittest_internal.h"

void unittest_packet();
void unittest_connection();


void unittest()
{
	puts("==> Start unittest");
	unittest_packet();
	unittest_connection();
	puts("<== Unittest completed");
}

