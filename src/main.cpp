#include <iostream>
#include "core/connection.h"
#include "core/packet.h"
#include "gui/gui.h"
#include <string.h>

void unittest();

int main(int argc, char *argv[])
{
	if (argc >= 2 && strcmp(argv[1], "--unittest") == 0) {
		unittest();
	}

	Gui gui;
	gui.run();

	puts("Hello World");
	return EXIT_SUCCESS;
}
