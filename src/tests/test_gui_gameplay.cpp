#if BUILD_CLIENT

#include "unittest_internal.h"
#include "gui/gameplay/gameplay.h"

void unittest_gui_gameplay()
{
	u8 param;

	CHECK(SceneGameplay::pianoNoteToParam("C3", &param));
	CHECK(param == 0);

	CHECK(!SceneGameplay::pianoNoteToParam("P3", &param));
	CHECK(!SceneGameplay::pianoNoteToParam("A9", &param));
	CHECK(!SceneGameplay::pianoNoteToParam("", &param));

	CHECK(SceneGameplay::pianoNoteToParam("d#3", &param));
	CHECK(param == (3 - 3) * 12 + 3); // C, C#, D, D#

	CHECK(SceneGameplay::pianoNoteToParam(" cb 5 ", &param));
	CHECK(param == (5 - 3) * 12 + 0 - 1); // B4
}

#else // BUILD_CLIENT

#include <stdio.h>

void unittest_gui_gameplay()
{
	puts("Not implemented");
}

#endif
