#include <iostream>
#include "core/connection.h"
#include "core/packet.h"
#include "gui/gui.h"
#include <string.h>

#include <chrono>
#include <thread>
void sleep_ms(long delay)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

void unittest();

int main(int argc, char *argv[])
{
	if (argc >= 2 && strcmp(argv[1], "--unittest") == 0) {
		unittest();
		return EXIT_SUCCESS;
	}

	Gui gui;
	gui.run();

	return EXIT_SUCCESS;
}
