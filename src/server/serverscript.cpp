#include "serverscript.h"
#include "core/script/script_utils.h"
#include "core/player.h"
#include "core/world.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

void ServerScript::onScriptsLoaded()
{
	Script::onScriptsLoaded();

	lua_State *L = m_lua;

	lua_getglobal(L, "env");
	lua_getfield(L, -1, "world");
	function_ref_from_field(L, -1, "on_player_join", m_ref_on_player_join, true);
	function_ref_from_field(L, -1, "on_player_leave", m_ref_on_player_leave, true);
	lua_pop(L, 2); // env + world
}

int ServerScript::implWorldSetTile(PositionRange range, bid_t block_id, int tile)
{
	lua_State *L = m_lua;
	World *world = m_player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	world->setBlockTiles(range, block_id, tile);
	// TODO: broadcast
	return 0;
}

static void run_0_args_callback(lua_State *L, int ref, const char *dbg)
{
	if (ref <= LUA_NOREF) {
		logger(LL_DEBUG, "0 arg callback unavailable. name='%s'", dbg);
		return;
	}

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	if (lua_pcall(L, 0, 0, 0)) {
		logger(LL_ERROR, "%s failed: %s\n",
			dbg,
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}

void ServerScript::onPlayerJoin(Player *player)
{
	setPlayer(player);
	run_0_args_callback(m_lua, m_ref_on_player_join, "on_player_join");
}

void ServerScript::onPlayerLeave(Player *player)
{
	setPlayer(player);
	run_0_args_callback(m_lua, m_ref_on_player_leave, "on_player_leave");
}
