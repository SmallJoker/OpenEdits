#include "unittest_internal.h"

#include "core/blockmanager.h"
#include "core/connection.h" // PROTOCOL_VERSION_*
#include "core/script/script.h"
#include "server/remoteplayer.h"


void unittest_script()
{
	BlockManager bmgr;

	Script script(&bmgr);
	script.do_load_string_n_table = true;
	CHECK(script.init());
	script.setTestMode("init");
	CHECK(script.loadFromFile("assets/scripts/main.lua"));
	CHECK(script.loadFromFile("assets/scripts/unittest.lua"));

	RemotePlayer p(12345, PROTOCOL_VERSION_MAX);
	script.setPlayer(&p);

	// on_intersect test. Multiple times to ensure the stack is OK
	for (int y = 10; y < 12; ++y) {
		p.pos.Y = y;
		script.setTestMode("set_pos");
		script.onIntersect(bmgr.getProps(2));
		CHECK(script.popErrorCount() == 0);
		CHECK(std::fabs(p.pos.Y - (y + 1)) < 0.001f);
	}

	// on_collide test
	{
		// fall onto the block (positive Y velocity)
		Script::CollisionInfo ci;
		ci.pos.X = 15;
		ci.pos.Y = 12;
		ci.is_x = false;
		ci.props = bmgr.getProps(2);

		p.pos = core::vector2df(
			ci.pos.X + 0,
			ci.pos.Y - 0.9
		);
		p.vel = core::vector2df(0, 20);
		CHECK(script.onCollide(ci) == (int)BlockProperties::CollisionType::Velocity);
		CHECK(script.popErrorCount() == 0);
	}

	script.close();
}
