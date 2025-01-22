#include "script.h"
#include "script_utils.h"
#include "core/blockmanager.h"
#include "core/macros.h"
#include "core/player.h"
#include "core/world.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

void Script::runCb_0(int ref, const char *dbg, int nargs)
{
	if (ref <= LUA_NOREF) {
		logger(LL_DEBUG, "callback '%s' = %d", dbg, ref);
		return;
	}

	lua_State *L = m_lua;

	int top = lua_gettop(L) - nargs;
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);

	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	/*
		Use the earlier pushed values as function args
		-4 : optional arg 1
		-3 : optional arg 2
		-2 : traceback
		-1 : function
	*/
	for (int i = 0; i < nargs; ++i)
		lua_pushvalue(L, -2 - nargs);

	if (lua_pcall(L, nargs, 0, (top + nargs) + 1)) {
		logger(LL_ERROR, "%s failed: %s\n",
			dbg,
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}

void Script::runBlockCb_0(int ref, const char *dbg)
{
	if (ref <= LUA_NOREF)
		return;

	lua_State *L = m_lua;

	int top = lua_gettop(L);
	// Function call prepration
	// This is faster than calling a getter function
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	// Execute!
	if (lua_pcall(L, 0, 0, 0)) {
		logger(LL_ERROR, "%s block=%d failed: %s\n",
			dbg, m_last_block_id,
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}

void Script::onBlockPlaced(bid_t block_id)
{
	const BlockProperties *props = m_bmgr->getProps(block_id);
	if (!props || !props->haveOnPlaced())
		return;

	m_last_block_id = block_id;
	runBlockCb_0(props->ref_on_placed, "on_placed");
}

void Script::onIntersect(const BlockProperties *props)
{
	if (!props || !props->haveOnIntersect()) {
		// no callback registered: fall-back to air
		props = m_bmgr->getProps(0);
	}

	m_last_block_id = props->id;
	runBlockCb_0(props->ref_on_intersect, "on_intersect");
}

void Script::onIntersectOnce(blockpos_t pos, const BlockProperties *props)
{
	lua_State *L = m_lua;
	m_last_block_id = props->id;

	if (!props || !props->haveOnIntersectOnce())
		return; // NOP

	Block block;
	if (m_world)
		m_world->getBlock(pos, &block);

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, props->ref_intersect_once);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushnumber(L, block.tile);

	// Execute!
	if (lua_pcall(L, 1, 0, 0)) {
		logger(LL_ERROR, "on_intersect_once block=%d failed: %s\n",
			props->id,
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}

int Script::onCollide(CollisionInfo ci)
{
	/*
		Performance measurements of a simple one-way gate
		LuaJIT 2.1-93e8799
		CPU: Intel 7th gen, at 800 MHz (fixed)
		Standing still, gravity = 100 m/s²
		60 FPS (less relevant)

		278 calls in 7 seconds -> 39.7 calls/s
		Mean: 5 µs
		Standard deviation: 1.39 µs

		-> This is perfectly fine.
	*/

	const BlockProperties *props = ci.props;
	using CT = BlockProperties::CollisionType;
	if (!props)
		return (int)CT::None;

	m_last_block_id = props->id;

	if (!props->haveOnCollide()) {
		ASSERT_FORCED(props->tiles[0].type != BlockDrawType::Solid,
			"onCollide called on solid");
		return (int)CT::None; // should not be returned: incorrect on solids
	}

	lua_State *L = m_lua;
	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, props->ref_on_collide);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushinteger(L, ci.pos.X);
	lua_pushinteger(L, ci.pos.Y);
	lua_pushboolean(L, ci.is_x);
	// Execute!
	if (lua_pcall(L, 3, 1, 0)) {
		logger(LL_ERROR, "on_collide block=%d failed: %s\n",
			m_last_block_id,
			lua_tostring(L, -1)
		);
		lua_settop(L, top);
		return (int)CT::None;
	}

	int collision_type = (int)CT::None;
	if (lua_isnumber(L, -1))
		collision_type = lua_tonumber(L, -1);
	else
		collision_type = -1; // invalid

	switch (collision_type) {
		case (int)CT::None:
		case (int)CT::Velocity:
		case (int)CT::Position:
			// good
			logger(LL_DEBUG, "collision_type=%i, pos=(%i,%i), dir=%s\n",
				collision_type, ci.pos.X, ci.pos.Y, ci.is_x ? "X" : "Y"
			);
			break;
		default:
			collision_type = 0;
			logger(LL_ERROR, "invalid collision in block=%d\n",
				m_last_block_id
			);
			break;
	}
	lua_settop(L, top);

	return collision_type;
}
