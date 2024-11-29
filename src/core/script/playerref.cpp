#include "playerref.h"
#include "script_utils.h"
#include "scriptevent.h"
#include "core/player.h"

static const char *CLASS_NAME = "PlayerRef";

static Logger logger("PlayerRef", LL_INFO);

void PlayerRef::doRegister(lua_State *L)
{
	logger(LL_DEBUG, __func__);

	static const luaL_Reg metatable_fn[] = {
		{"__gc", garbagecollect},
		{nullptr, nullptr}
	};
	static const luaL_Reg classtable_fn[] = {
		{"get_name", get_name},
		{"send_event", send_event},
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

	// Get existing PlayerRef
	lua_rawgeti(L, LUA_REGISTRYINDEX, ScriptUtils::CUSTOM_RIDX_PLAYER_REFS); // 1
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_rawgeti(L, -1, id); // 2
	if (lua_isuserdata(L, -1)) {
		// found :)
		lua_remove(L, -2); // table
		logger(LL_DEBUG, "%s recycled", __func__);
		return 1;
	}

	// No match: create a new instance
	lua_pop(L, 1); // (nil), 1

	PlayerRef *ref = new PlayerRef();
	ref->m_player = player;

	*(void **)lua_newuserdata(L, sizeof(void *)) = ref; // 2
	luaL_getmetatable(L, CLASS_NAME);
	lua_setmetatable(L, -2);

	lua_pushvalue(L, -1); // 3, copy PlayerRef
	lua_rawseti(L, -3, id); // 2, assign
	lua_remove(L, -2); // 1, table
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

	lua_rawgeti(L, LUA_REGISTRYINDEX, ScriptUtils::CUSTOM_RIDX_PLAYER_REFS);
	lua_rawgeti(L, -1, player->peer_id);
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

int PlayerRef::send_event(lua_State* L)
{
	MESSY_CPP_EXCEPTIONS_START
	PlayerRef *ref = toPlayerRef(L, 1);
	if (!ref->m_player)
		return 0;

	Player *player = ref->m_player;

	ScriptEvent ev = player->getSEMgr()->readEventFromLua(2);
	if (!player->script_events)
		player->script_events.reset(new ScriptEventList());

	player->script_events->emplace(std::move(ev));
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

