#include "playerref.h"
#include "script.h"
#include "script_utils.h"
#include "scriptevent.h"
#include "core/player.h"

using namespace ScriptUtils;

static const char *CLASS_NAME = "PlayerRef";

static Logger logger("PlayerRef", LL_INFO);

static void get_player_from_table(lua_State *L, peer_t id)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, ScriptUtils::CUSTOM_RIDX_PLAYER_REFS); // 1
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_rawgeti(L, -1, id); // 2
}

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

void PlayerRef::doRegister(lua_State *L)
{
	logger(LL_DEBUG, __func__);

	static const luaL_Reg metatable_fn[] = {
		{"__gc", garbagecollect},
		{nullptr, nullptr}
	};
	static const luaL_Reg classtable_fn[] = {
		{"get_name", get_name},
		{"hash", hash},
		{"send_event", send_event},
		{"next_prn", next_prn},
		{"get_pos", get_pos},
		{"set_pos", set_pos},
		{"get_vel", get_vel},
		{"set_vel", set_vel},
		{"get_acc", get_acc},
		{"set_acc", set_acc},
		{"get_controls", get_controls},
		{nullptr, nullptr}
	};

	luaL_newmetatable(L, CLASS_NAME); // 1
	luaL_register(L, nullptr, metatable_fn);

	lua_newtable(L); // 2
	luaL_register(L, nullptr, classtable_fn);

	// "Inherit" the methods. https://www.lua.org/pil/13.4.1.html "The __index Metamethod"
	lua_pushvalue(L, -1); // classtable
	lua_setfield(L, -3, "__index");

	// Prevent changes. https://www.lua.org/pil/13.3.html "Library-Defined Metamethods"
	// This hides the "__*" metatable fields
	lua_pushvalue(L, -1); // classtable
	lua_setfield(L, -3, "__metatable");

	lua_pop(L, 2); // classtable + metatable
}


int PlayerRef::push(lua_State *L, Player *player)
{
	const peer_t id = player->peer_id;
	get_player_from_table(L, id); // +2

	if (lua_isuserdata(L, -1)) {
		// found :)
		lua_remove(L, -2); // table
		logger(LL_DEBUG, "%s recycled", __func__);
		return 1;
	}

	// No match: create a new instance
	lua_pop(L, 1); // [-1] == (nil)

	PlayerRef *ref = new PlayerRef();
	ref->m_player = player;

	*(void **)lua_newuserdata(L, sizeof(void *)) = ref; // 2
	luaL_getmetatable(L, CLASS_NAME);
	lua_setmetatable(L, -2);

	lua_pushvalue(L, -1); // 3, copy PlayerRef to top
	lua_rawseti(L, -3, id); // 2, assign
	lua_remove(L, -2); // 1, RIDX table
	toPlayerRef(L, -1); // sanity check
	logger(LL_DEBUG, "%s new", __func__);
	return 1;
}

bool PlayerRef::invalidate(lua_State *L, Player *player)
{
	logger(LL_DEBUG, __func__);
	const peer_t id = player->peer_id;
	bool removed = false;

	int top = lua_gettop(L);

	get_player_from_table(L, id); // +2
	{
		if (lua_isuserdata(L, -1)) {
			PlayerRef *ref = toPlayerRef(L, -1);
			ref->m_player = nullptr;
			removed = true;
		}
		lua_pop(L, 1); // ref
	}
	lua_pushnil(L);
	lua_rawseti(L, -2, id);

	lua_settop(L, top);
	return removed;
}


// -------------- Lua API functions -------------


PlayerRef *PlayerRef::toPlayerRef(lua_State *L, int idx)
{
	return *(PlayerRef **)luaL_checkudata(L, idx, CLASS_NAME);
}

int PlayerRef::garbagecollect(lua_State *L)
{
	logger(LL_DEBUG, __func__);
	PlayerRef *ref = toPlayerRef(L, 1);
	delete ref;
	return 0;
}

int PlayerRef::get_name(lua_State *L)
{
	logger(LL_DEBUG, __func__);
	PlayerRef *ref = toPlayerRef(L, 1);
	if (!ref->m_player)
		return 0;

	lua_pushstring(L, ref->m_player->name.c_str());
	return 1;
}

int PlayerRef::hash(lua_State *L)
{
	PlayerRef *ref = toPlayerRef(L, 1);
	if (!ref->m_player)
		return 0;

	lua_pushinteger(L, ref->m_player->peer_id);
	return 1;
}

int PlayerRef::send_event(lua_State *L)
{
	// Player-specific events
	MESSY_CPP_EXCEPTIONS_START
	Player *player = toPlayerRef(L, 1)->m_player;
	if (!player)
		return 0;

	Script *script = player->getScript();
	script->implSendEvent({ .player = player }, true);
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

int PlayerRef::next_prn(lua_State* L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (!player)
		return 0;

	lua_pushinteger(L, player->getNextPRNum());
	return 1;
}


// -------------- Physics / controls --------------


int PlayerRef::get_pos(lua_State *L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->pos);
	return 2;
}

int PlayerRef::set_pos(lua_State *L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (player)
		pull_v2f(L, 2, player->pos);
	return 0;
}

int PlayerRef::get_vel(lua_State *L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->vel);
	return 2;
}

int PlayerRef::set_vel(lua_State *L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (player)
		pull_v2f(L, 2, player->vel);
	return 0;
}

int PlayerRef::get_acc(lua_State *L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->acc);
	return 2;
}

int PlayerRef::set_acc(lua_State *L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (player)
		pull_v2f(L, 2, player->acc);
	return 0;
}

int PlayerRef::get_controls(lua_State *L)
{
	Player *player = toPlayerRef(L, 1)->m_player;
	if (!player)
		return 0;

	// TODO: caching
/*
	static int cnt_new = 0,
		cnt_cache = 0;
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PLAYER_CONTROLS);

	// Cache hits for one-way gates: about 5 % (not very great)
	// Best case scenario: about 32 % (multi-block wide columns)
	if (0)
		printf("cache hits: %d %%\n", (cnt_cache * 100) / (cnt_new + cnt_cache));

*/
	lua_newtable(L);
	{
		const PlayerControls &controls = player->getControls();
		lua_pushboolean(L, controls.jump);
		lua_setfield(L, -2, "jump");

		// atan2(Y, X) is in CW rotation, ∈ ]-PI, PI]
		// 0   * PI : to the left (+X direction
		// 0.5 * PI : downwards   (+Y direction)
		lua_pushnumber(L, controls.dir.X);
		lua_setfield(L, -2, "dir_x");
		lua_pushnumber(L, controls.dir.Y);
		lua_setfield(L, -2, "dir_y");
	}
	return 1;
}
