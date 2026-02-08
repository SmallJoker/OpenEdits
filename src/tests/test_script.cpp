#include "unittest_internal.h"

#include "core/blockmanager.h"
#include "core/connection.h" // PROTOCOL_VERSION_*
#include "core/packet.h"
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "core/script/scriptevent.h"
#include "core/world.h"
#include "core/worldmeta.h"
#include "server/remoteplayer.h"
#include "server/serverscript.h"

static int testprint(lua_State *L)
{
	ScriptUtils::dump_args(L, stdout, true);
	puts("");
	return 0;
}

static bool run_script(lua_State *L, const char *text, int line)
{
	int status = luaL_dostring(L, text);
	if (status != 0) {
		printf("Error @ line %d: %s", line, lua_tostring(L, -1));
		return false;
	}
	return true;
}

static void test_playerref()
{
	lua_State *L = lua_open();
	luaopen_base(L);

	lua_pushcfunction(L, testprint);
	lua_setglobal(L, "print");

	PlayerRef::doRegister(L);

	// Needed by PlayerRef calls
	lua_newtable(L);
	lua_rawseti(L, LUA_REGISTRYINDEX, ScriptUtils::CUSTOM_RIDX_PLAYER_REFS);

	RemotePlayer p(1009, 10);
	p.name = "FooMcBar";

	for (int i = 0; i < 2; ++i) {
		PlayerRef::push(L, &p);
		lua_setglobal(L, "player");
	}

	bool ok = run_script(L, R"EOF(
		t = getmetatable(player)
		--rawset(t, "__gc", "test") -- does not affect PlayerRef::garbagecollect
		for k, v in pairs(t) do
			print(k, v)
		end
		i = rawget(t, "__index")
		assert(i == nil, "index leak")
		print(player:get_name())
	)EOF", __LINE__);
	CHECK(ok);

	CHECK(PlayerRef::invalidate(L, &p));  // invalidated and removed
	CHECK(!PlayerRef::invalidate(L, &p)); // nothing to do

	if (0) {
		// Force PlayerRef::garbagecollect
		lua_pushnil(L);
		lua_setglobal(L, "player");
		lua_gc(L, LUA_GCCOLLECT, 0);
	}

	ok = run_script(L, R"EOF(
		assert(player:get_name() == nil)
	)EOF", __LINE__);
	CHECK(ok);

	// done
	lua_close(L);
}


namespace {
	class LeakingServerScript : public ServerScript {
	public:
		inline bool callFunction_pub(int ref, int nres, const char *dbg, int nargs, bool is_block = false)
		{
			return callFunction(ref, nres, dbg, nargs, is_block);
		}

		inline lua_State *getState() const
		{
			return m_lua;
		}
	};
}

static void test_utilities()
{
	BlockManager bmgr;
	ServerScript script_private(&bmgr, nullptr);
	LeakingServerScript &script = *(LeakingServerScript *)&script_private;

	script.hide_global_table = false;
	CHECK(script.init());
	lua_State *L = script.getState();

	int status = luaL_dostring(L, R"EOF(
		function dummy(...)
			local t = {...}
			for k, v in pairs(t) do
				print(k, v)
			end
			return t[1], "AA", 123
		end
	)EOF");
	if (status != 0) {
		puts(lua_tostring(L, -1));
		CHECK(0);
	}

	int fn_dummy;
	{
		lua_getglobal(L, "_G");
		ScriptUtils::function_ref_from_field(L, -1, "dummy", fn_dummy);
		lua_pop(L, -1); // _G
	}

	{
		int top = lua_gettop(L);
		lua_pushinteger(L, 100);
		lua_pushstring(L, "dummy");
		bool ok = script.callFunction_pub(fn_dummy, 3, "dummy", 2, false);
		CHECK(ok);
		ScriptUtils::dump_args(L, stdout, true);
		//ScriptUtils::dump_args(L, stdout, true);
		// Stack: 2 original args + traceback func + 3 returned values
		CHECK(lua_gettop(L) == 2 + 1 + 3);

		// Warning! The last argument comes first! (stack)
		CHECK(lua_tonumber(L, -1) == 123);
		CHECK(lua_tonumber(L, -3) == 100);
		lua_settop(L, top);
	}
}


static void test_player_physics(Script &script, Player &p)
{
	auto &bmgr = *script.getBlockMgr();

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
}

static void test_basic_scriptevents(Script &script, Player &p)
{
	auto &bmgr = *script.getBlockMgr();
	auto w = p.getWorld();
	CHECK(w); // player must have joined a world

	// script events: out queue
	auto *smgr = script.getSEMgr();

	// make the player send an event
	script.onIntersect(bmgr.getProps(4));
	auto myevents = w->getMeta().script_events_to_send.get();

	CHECK(myevents && myevents->size() == 1);
	auto se = myevents->begin();
	CHECK(se->first == 1003 && se->second.data.size() == 2);

	// run callback function
	smgr->runLuaEventCallback(*myevents->begin());
	CHECK(script.popTestFeedback() == "hello world200103;");

	{
		// Serialize/deserialize
		Packet pkt;
		smgr->writeBatchNT(pkt, true, myevents);
		pkt.write<u16>(UINT16_MAX);

		ScriptEvent se;
		size_t count = 0;
		while (smgr->readNextEvent(pkt, true, se)) {
			switch (count) {
				case 1:
					CHECK(se.first == 1003);
					CHECK(se.second.data.size() == 2);
				break;
			}
			count++;
		}
		CHECK(count == 1)
	}
}

static void test_playerref_scriptevents(Script &script, Player &p)
{
	const BlockManager *bmgr = script.getBlockMgr();

	{
		auto props = bmgr->getProps(4);
		CHECK(props != nullptr);
		script.onIntersectOnce({0 , 0}, props);
		CHECK(script.popErrorCount() == 0);
		CHECK(p.script_events_to_send);
		ScriptEventMap list = std::move(*p.script_events_to_send);
		CHECK(list.begin()->first == (1004 | SEF_HAVE_ACTOR));

		// Process the added event
		script.getSEMgr()->runLuaEventCallback(*list.begin());
		CHECK(script.popErrorCount() == 0);
		CHECK(script.popTestFeedback() == "EV4.15577;");
	}
}

static void test_player_callbacks(BlockManager *bmgr, Script &script, RemotePlayer &p)
{
	auto w = std::make_shared<World>(bmgr, "wcallbacks");
	p.setWorld(w);
	CHECK(p.getScript());

	// join/leave
	{
		script.onPlayerEvent("join", &p);
		CHECK(script.popTestFeedback() == "J_SRV;");
		script.onPlayerEvent("leave", &p);
		CHECK(script.popTestFeedback() == "L_SRV;");
	}

	script.setTestMode("wdata");
	{
		script.onPlayerEvent("test_check_wdata", &p);
		CHECK(script.popTestFeedback() == "NIL;");

		script.onPlayerEvent("join", &p);
		script.onPlayerEvent("test_check_wdata", &p);
		CHECK(script.popTestFeedback() == "J_SRV;>0;"); // "foobar_key"

		script.onPlayerEvent("leave", &p);
		script.onPlayerEvent("test_check_wdata", &p);
		CHECK(script.popTestFeedback() == "L_SRV;NIL;");
	}

	p.setWorld(nullptr);
}

static void test_block_placement(BlockManager *bmgr, Script *script, RemotePlayer &p)
{
	auto w = std::make_shared<World>(bmgr, "interop");
	w->createEmpty({ 10, 7 });
	w->setBlock({ 5, 6 }, Block(101)); // center bottom

	p.setWorld(w);
	CHECK(p.getScript());

	// env.world.set_tile
	{
		p.setPosition({ 5.0f, 4.2f }, false);
		p.step(0.01f); // update acceleration etc.
		p.step(0.3f);
		CHECK(std::fabs(p.pos.Y - 5.0f) < 0.001f);
		Block b;
		w->getBlock({ 5, 6 }, &b);
		CHECK(b.tile == 1);
	}
	CHECK(script->popErrorCount() == 0);

	// env.world.get_blocks_in_range
	{
		{
			BlockUpdate bu(bmgr);
			bu.pos = blockpos_t(7, 6);
			bu.set(102);
			w->updateBlock(bu);
		}

		p.setPosition({ 7.1f, 5.4f }, false);
		p.step(0.01f);
		p.step(0.3f); // call on_intersect_once
		//printf("%.3f %.3f\n", p.pos.X, p.pos.Y);

		// expected: 2 blocks, 1x 4 values, 1x 7 values
		CHECK(script->popTestFeedback() == "called_102 2 4 7;");
	}
	CHECK(script->popErrorCount() == 0);

	// env.on_block_place
	{
		BlockUpdate bu(bmgr);
		bu.pos = blockpos_t(7, 5);
		bu.set(104);

		w->setBlock(bu.pos, Block(12));

		CHECK(script->onBlockPlace(bu) == true);
		CHECK(script->popTestFeedback() == p.name + ":12:0;");
		w->updateBlock(bu);

		bu.set(107);
		CHECK(script->onBlockPlace(bu) == false);
	}
	p.setWorld(nullptr);
}

void unittest_script()
{
	test_playerref();
	test_utilities();

	BlockManager bmgr;

	ServerScript script(&bmgr, nullptr);
	script.hide_global_table = false;
	CHECK(script.init());
	script.setTestMode("init");
	CHECK(script.loadFromFile("assets/scripts/constants.lua"));
	CHECK(script.loadFromFile("assets/scripts/unittest.lua"));
	CHECK(script.loadFromFile("assets/scripts/player_data.lua"));
	CHECK(script.getScriptType() == Script::ST_SERVER);
	script.onScriptsLoaded();

	RemotePlayer p(12345, PROTOCOL_VERSION_MAX);
	p.name = "MCFOOBAR";
	script.setPlayer(&p);

	p.setScript(&script);
	CHECK(p.getScript() == nullptr); // until a world is assigned

	test_player_physics(script, p);

	// script events
	{
		auto w = std::make_shared<World>(&bmgr, "wscriptevent");
		p.setWorld(w);
		script.setPlayer(&p); // now with a world

		test_basic_scriptevents(script, p);

		test_playerref_scriptevents(script, p);
		p.setWorld(nullptr);
	}

	test_player_callbacks(&bmgr, script, p);

	test_block_placement(&bmgr, &script, p);

	script.close();
}
