#include "script.h"
#include "script_utils.h"
#include "core/player.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

static void push_v2f(lua_State *L, core::vector2df vec)
{
	lua_pushnumber(L, vec.X);
	lua_pushnumber(L, vec.Y);
}

static void pull_v2f(lua_State *L, int idx, core::vector2df &vec)
{
	if (!lua_isnil(L, idx))
		vec.X = luaL_checknumber(L, idx);
	if (!lua_isnil(L, idx + 1))
		vec.Y = luaL_checknumber(L, idx + 1);
}

int Script::l_player_get_pos(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->pos);
	return 2;
}

int Script::l_player_set_pos(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (player)
		pull_v2f(L, 1, player->pos);
	return 0;
}

int Script::l_player_get_vel(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->vel);
	return 2;
}

int Script::l_player_set_vel(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (player)
		pull_v2f(L, 1, player->vel);
	return 0;
}

int Script::l_player_get_acc(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->acc);
	return 2;
}

int Script::l_player_set_acc(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (player)
		pull_v2f(L, 1, player->acc);
	return 0;
}

int Script::l_player_get_controls(lua_State *L)
{
	static int cnt_new = 0,
		cnt_cache = 0;

	Script *script = get_script(L);
	Player *player = script->m_player;

	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PLAYER_CONTROLS);

	if (!script->m_player_controls_cached && player) {
		script->m_player_controls_cached = true;

		const PlayerControls &controls = player->getControls();
		lua_pushboolean(L, controls.jump);
		lua_setfield(L, -2, "jump");

		// Goes in CW rotation, ∈ ]-PI, PI]
		// 0   * PI : to the left (+X direction
		// 0.5 * PI : downwards   (+Y direction)
		lua_pushnumber(L, std::atan2(controls.dir.Y, controls.dir.X));
		lua_setfield(L, -2, "angle");

		// May exceed 1 when two keys are pressed
		lua_pushnumber(L, std::hypot(controls.dir.Y, controls.dir.X));
		lua_setfield(L, -2, "factor");

		cnt_new++;
	} else {
		// Table reference
		cnt_cache++;
	}

	// Cache hits for one-way gates: about 5 % (not very great)
	// Best case scenario: about 32 % (multi-block wide columns)
	if (0)
		printf("cache hits: %d %%\n", (cnt_cache * 100) / (cnt_new + cnt_cache));

	return 1;
}

int Script::l_player_get_name(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (!player)
		return 0;

	lua_pushstring(L, player->name.c_str());
	return 1;
}

int Script::l_player_hash(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (!player)
		return 0;

	lua_pushinteger(L, player->peer_id);
	return 1;
}
