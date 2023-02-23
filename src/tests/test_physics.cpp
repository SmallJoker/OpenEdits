#include "unittest_internal.h"
#include "core/world.h"
#include "server/remoteplayer.h"

static bool fuzzy_check(core::vector2df a, core::vector2df b, float maxdiff)
{
	float diff = (a - b).getLengthSQ();
	if (diff < maxdiff * maxdiff)
		return true;
	printf("Expected: (%g,%g), got (%g,%g)\n", b.X, b.Y, a.X, a.Y);
	return false;
}

static void run_steps(Player &p, float dtime)
{
	constexpr float stepsize = 0.3f; // seconds
	while (dtime > stepsize) {
		p.step(stepsize);
		dtime -= stepsize;
	}
	p.step(dtime);
}

void unittest_physics()
{
	// Run physics simulations to check whether the player movement works as expected

	Block b_left(1);
	Block b_up(2);
	Block b_right(3);
	Block b_0g(4);

	RemotePlayer p1(1337, 42);
	{
		World w1("physics1");
		w1.createEmpty(blockpos_t(8, 11));
		p1.setWorld(&w1);

		// Must reach the ground
		run_steps(p1, 2);
		CHECK(fuzzy_check(p1.pos, {0, 10}, 0.1f));

		// No motion after respawn
		p1.setPosition({0, 0}, true);
		w1.setBlock({0, 0}, b_0g);
		run_steps(p1, 2);
		CHECK(fuzzy_check(p1.pos, {0, 0}, 0.1f));

		// Horizontal >>><<<
		w1.setBlock({0, 0}, Block());
		for (int i = 1; i <= 6; ++i)
			w1.setBlock(blockpos_t(i, 0), i <= 3 ? b_right : b_left);
		p1.setPosition({1, 0}, true);
		run_steps(p1, 10);
		CHECK(fuzzy_check(p1.pos, {3.5, 0}, 3.0f));

		// Vertical ____<< (unstable)
		w1.setBlock({0, 4}, b_up);
		w1.setBlock({0, 5}, b_up);
		p1.setPosition({0, 4}, true);
		run_steps(p1, 10);
		CHECK(fuzzy_check(p1.pos, {0, 10}, 0.1f));

		// Vertical ____<<< (stable)
		w1.setBlock({0, 6}, b_up);
		p1.setPosition({0, 4}, true);
		run_steps(p1, 10);
		CHECK(fuzzy_check(p1.pos, {0, 5}, 3.99f));

		p1.setWorld(nullptr);
	}
}
