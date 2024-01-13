#include "unittest_internal.h"

#if BUILD_CLIENT
#include "gui/sound.h"

void sleep_ms(long delay);

void unittest_sound()
{
	SoundPlayer sp(true);

	sp.play("coin.mono_s16.raw", {0, 0});
	sleep_ms(500);
	sp.play("coin.mono_s16.raw", {0, 0}, 1.5f);
	sleep_ms(500);

	sp.step();
}

#else

void unittest_sound()
{
	puts("Sound not available");
}

#endif
