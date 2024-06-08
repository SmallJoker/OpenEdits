#include "script.h"
#include "core/blockmanager.h"
#include "core/logger.h"
#include <core/macros.h>

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

extern Logger script_logger;
static Logger &logger = script_logger;


bool Script::haveOnIntersect(const BlockProperties *props) const
{
	return props && props->ref_on_intersect >= 0;
}

void Script::onIntersect(const BlockProperties *props)
{
	lua_State *L = m_lua;
	m_last_block_id = props->id;

	if (!haveOnIntersect(props)) {
		// no callback registered: fall-back to air
		props = m_bmgr->getProps(0);
	}

	int top = lua_gettop(L);
	// Function call prepration
	// This is faster than calling a getter function
	lua_rawgeti(L, LUA_REGISTRYINDEX, props->ref_on_intersect);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	// Execute!
	if (lua_pcall(L, 0, 0, 0)) {
		logger(LL_ERROR, "on_intersect block=%d failed: %s\n",
			props->id,
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}


bool Script::haveOnIntersectOnce(const BlockProperties *props) const
{
	return props && props->ref_intersect_once >= 0;
}

void Script::onIntersectOnce(const BlockProperties *props)
{
	lua_State *L = m_lua;
	m_last_block_id = props->id;

	if (!haveOnIntersectOnce(props))
		return; // NOP

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, props->ref_intersect_once);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	// Execute!
	if (lua_pcall(L, 0, 0, 0)) {
		logger(LL_ERROR, "on_intersect_once block=%d failed: %s\n",
			props->id,
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}


bool Script::haveOnCollide(const BlockProperties *props) const
{
	return props && props->ref_on_collide >= 0;
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

	lua_State *L = m_lua;
	m_last_block_id = ci.props->id;
	using CT = BlockProperties::CollisionType;

	if (!haveOnCollide(ci.props)) {
		ASSERT_FORCED(ci.props->tiles[0].type != BlockDrawType::Solid,
			"Should not be called.");
		return (int)CT::None; // should not be returned: incorrect on solids
	}

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ci.props->ref_on_collide);
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
