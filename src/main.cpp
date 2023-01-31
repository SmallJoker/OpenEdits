#include <iostream>
#include "core/connection.h"
#include "core/packet.h"
#include "gui/render.h"

void unittest();
void lobby_gui();

int main(int argc, char *argv[])
{
	//unittest();
	lobby_gui();
	Render render;


	puts("Hello World");
	return EXIT_SUCCESS;
}
