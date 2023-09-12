#include "unittest_internal.h"
#include "core/world.h"
#include "server/remoteplayer.h"

static bool fuzzy_check(core::vector2df a, core::vector2df b, float maxdiff = 0.1f)
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
		// Test gravity arrows
		auto w1_obj = std::make_shared<World>(g_blockmanager, "physics_arrows");
		World &w1 = *w1_obj.get();
		w1.createEmpty(blockpos_t(8, 11));
		p1.setWorld(w1_obj);

		// Must reach the ground
		run_steps(p1, 2);
		CHECK(fuzzy_check(p1.pos, {0, 10}));

		// No motion after respawn
		p1.setPosition({0, 0}, true);
		w1.setBlock({0, 0}, b_0g);
		run_steps(p1, 2);
		CHECK(fuzzy_check(p1.pos, {0, 0}));

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
		CHECK(fuzzy_check(p1.pos, {0, 10}));

		// Vertical ____<<< (stable)
		w1.setBlock({0, 6}, b_up);
		p1.setPosition({0, 4}, true);
		run_steps(p1, 10);
		CHECK(fuzzy_check(p1.pos, {0, 5}, 3.99f));

		p1.setWorld(nullptr);
	}

	Block b_solid(9);
	{
		// Test player inputs
		auto w2_obj = std::make_shared<World>(g_blockmanager, "physics_ctrl");
		World &w2 = *w2_obj.get();
		w2.createEmpty(blockpos_t(6, 6));
		p1.setWorld(w2_obj);

		// 1x3 pillar
		w2.setBlock({2, 3}, b_solid);
		w2.setBlock({2, 4}, b_solid);
		w2.setBlock({2, 5}, b_solid);
		p1.setPosition({0, 0}, true);

		PlayerControls ctrl;
		ctrl.dir.X = 1; // move left
		ctrl.jump = true;
		p1.setControls(ctrl);

		run_steps(p1, 2);
		p1.setControls(PlayerControls());
		run_steps(p1, 2);

		CHECK(fuzzy_check(p1.pos, {5, 5}));

		// 1x4 pillar
		w2.setBlock({2, 2}, b_solid);
		p1.setPosition({0, 0}, true);

		p1.setControls(ctrl);

		run_steps(p1, 2);
		p1.setControls(PlayerControls());
		run_steps(p1, 2);

		CHECK(fuzzy_check(p1.pos, {1, 5}));

		p1.setWorld(nullptr);
	}
}
