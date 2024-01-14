#include "unittest_internal.h"

#if BUILD_CLIENT
#include "gui/sound.h"

void sleep_ms(long delay);

void unittest_sound()
{
	SoundPlayer sp(true);

	{
		SoundSpec spec("piano_c4");
		sp.play(spec);
		sleep_ms(500);
	}
	{
		SoundSpec spec("piano_c4");
		spec.pitch = 1.5f;
		sp.play(spec);
		sleep_ms(500);
	}

	sp.step();
}

#else

void unittest_sound()
{
	puts("Sound not available");
}

#endif
