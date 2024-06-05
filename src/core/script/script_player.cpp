#include "script.h"
#include "script_utils.h"
#include "core/player.h"

using namespace ScriptUtils;

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
