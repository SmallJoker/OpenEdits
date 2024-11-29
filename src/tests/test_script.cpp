#include "unittest_internal.h"

#include "core/blockmanager.h"
#include "core/connection.h" // PROTOCOL_VERSION_*
#include "core/packet.h"
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "core/script/scriptevent.h"
#include "core/world.h"
#include "server/remoteplayer.h"
#include "server/serverscript.h"

static int testprint(lua_State *L)
{
	ScriptUtils::dump_args(L, stdout, true);
	puts("");
	return 0;
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

	int status = luaL_dostring(L, R"EOF(
		t = getmetatable(player)
		--rawset(t, "__gc", "test") -- does not affect PlayerRef::garbagecollect
		for k, v in pairs(t) do
			print(k, v)
		end
		i = rawget(t, "__index")
		assert(i == nil, "index leak")
		print(player:get_name())
	)EOF");
	if (status != 0) {
		puts(lua_tostring(L, -1));
		CHECK(0);
	}

	CHECK(PlayerRef::invalidate(L, &p));  // invalidated and removed
	CHECK(!PlayerRef::invalidate(L, &p)); // nothing to do

	if (0) {
		// Force PlayerRef::garbagecollect
		lua_pushnil(L);
		lua_setglobal(L, "player");
		lua_gc(L, LUA_GCCOLLECT, 0);
	}

	status = luaL_dostring(L, R"EOF(
		assert(player:get_name() == nil)
	)EOF");
	if (status != 0) {
		puts(lua_tostring(L, -1));
		CHECK(0);
	}

	// done
	lua_close(L);
}

static void test_playerref_scriptevents(Script &script, Player &p)
{
	lua_State *L = script.getState();
	const BlockManager *bmgr = script.getBlockMgr();
	PlayerRef::doRegister(L);

	PlayerRef::push(L, &p);
	lua_setglobal(L, "myplayerref");
	p.setScript(&script);

	{
		auto props = bmgr->getProps(4);
		CHECK(props != nullptr);
		script.onIntersectOnce({0 , 0}, props);
		CHECK(script.popErrorCount() == 0);
		CHECK(p.script_events.get());
		std::unique_ptr<ScriptEventList> list(p.script_events.release());
		CHECK(list->begin()->event_id == 1004);

		// Process the added event
		script.getSEMgr()->runLuaEventCallback(*list->begin());
		CHECK(script.popErrorCount() == 0);
		CHECK(script.popTestFeedback() == "EV4.15577;");
	}

	p.setScript(nullptr);
	PlayerRef::invalidate(L, &p);
}

static void test_script_world_interop(BlockManager *bmgr, Script *script, RemotePlayer &p)
{
	auto w = std::make_shared<World>(bmgr, "interop");
	w->createEmpty({ 10, 7 });
	w->setBlock({ 5, 6 }, Block(101)); // center bottom

	// env.world.set_tile
	{
		p.setScript(script);
		p.setWorld(w);
		p.setPosition({ 5.0f, 4.2f }, false);
		p.step(0.01f); // update acceleration etc.
		p.step(0.3f);
		CHECK(std::fabs(p.pos.Y - 5.0f) < 0.001f);
		Block b;
		w->getBlock({ 5, 6 }, &b);
		CHECK(b.tile == 1);

		p.setWorld(nullptr);
	}
	CHECK(script->popErrorCount() == 0);
}

void unittest_script()
{
	test_playerref();

	BlockManager bmgr;

	ServerScript script(&bmgr, nullptr);
	script.hide_global_table = false;
	CHECK(script.init());
	script.setTestMode("init");
	CHECK(script.loadFromFile("assets/scripts/constants.lua"));
	CHECK(script.loadFromFile("assets/scripts/unittest.lua"));
	CHECK(script.getScriptType() == Script::ST_SERVER);
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
		// make the player send an event
		p.script_events.reset(new ScriptEventList());
		auto &myevents = *p.script_events.get();

		script.onIntersect(bmgr.getProps(4));
		CHECK(myevents.size() == 1);
		auto se = myevents.begin();
		CHECK(se->event_id == 1003 && se->data->size() == 2 + 11 + 3);

		// run callback function
		script.getSEMgr()->runLuaEventCallback(*myevents.begin());
		CHECK(script.popTestFeedback() == "hello world200103;");

		p.script_events.reset();
	}

	test_playerref_scriptevents(script, p);

	// join/leave
	{
		script.onPlayerJoin(&p);
		CHECK(script.popTestFeedback() == "J_SRV;");
		script.onPlayerLeave(&p);
		CHECK(script.popTestFeedback() == "L_SRV;");
	}

	test_script_world_interop(&bmgr, &script, p);

	script.close();
}
