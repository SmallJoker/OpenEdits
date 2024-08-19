#include "unittest_internal.h"

#include "core/blockmanager.h"
#include "core/connection.h" // PROTOCOL_VERSION_*
#include "core/packet.h"
#include "server/remoteplayer.h"
#include "server/serverscript.h"


void unittest_script()
{
	BlockManager bmgr;

	ServerScript script(&bmgr);
	script.do_load_string_n_table = true;
	CHECK(script.init());
	script.setTestMode("init");
	CHECK(script.loadFromFile("assets/scripts/constants.lua"));
	CHECK(script.loadFromFile("assets/scripts/unittest.lua"));
	CHECK(script.loadFromFile("assets/scripts/unittest_server.lua"));
	script.onScriptsLoaded();

	RemotePlayer p(12345, PROTOCOL_VERSION_MAX);
	p.name = "MCFOOBAR";
	script.setPlayer(&p);

	// on_intersect_once
	{
		script.setTestMode("py set +1");
		float y = p.pos.Y;
		blockpos_t pos = p.getCurrentBlockPos();
		script.onIntersectOnce(pos, bmgr.getProps(2));
		CHECK(std::fabs(p.pos.Y - (y + 1)) < 0.001f);
	}

	// on_intersect test. Multiple times to ensure the stack is OK
	script.setTestMode("py set -1");
	for (int y = 10; y < 12; ++y) {
		p.pos.Y = y;
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

	// script events
	{

		std::set<ScriptEvent> myevents;

		// make the player send an event
		{
			p.event_list = &myevents;

			script.onIntersect(bmgr.getProps(4));
			CHECK(myevents.size() == 1);
			auto se = myevents.begin();
			CHECK(se->event_id == 1003 && se->data->size() == 2 + 11 + 3);
			p.event_list = nullptr;
		}

		// run callback function
		script.onEvent(*myevents.begin());
		CHECK(script.getTestFeedback() == ";hello world200103");
	}

	// join/leave
	{
		script.onPlayerJoin(&p);
		CHECK(script.getTestFeedback() == "J_COM;J_SRV;");
		script.onPlayerLeave(&p);
		CHECK(script.getTestFeedback() == "L_COM;L_SRV;");
	}

	script.close();
}
