#include "serverscript.h"
#include "remoteplayer.h"
#include "server.h"
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "core/world.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

void ServerScript::onScriptsLoaded()
{
	Script::onScriptsLoaded();

	lua_State *L = m_lua;

	PlayerRef::doRegister(L);

	lua_getglobal(L, "env");
	lua_getfield(L, -1, "server");
	field_set_function(L, "get_players_in_world", ServerScript::l_get_players_in_world);

	function_ref_from_field(L, -1, "on_player_join", m_ref_on_player_join, false);
	function_ref_from_field(L, -1, "on_player_leave", m_ref_on_player_leave, false);
	lua_pop(L, 2); // env + server
}

void ServerScript::initSpecifics()
{
	lua_State *L = m_lua;
	lua_newtable(L);
	lua_setfield(L, -2, "server");
}


int ServerScript::implWorldSetTile(PositionRange range, bid_t block_id, int tile)
{
	lua_State *L = m_lua;
	World *world = m_player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	// TODO: broadcast?

	bool modified = world->setBlockTiles(range, block_id, tile);
	lua_pushboolean(L, modified);
	return 1;
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

int ServerScript::l_get_players_in_world(lua_State *L)
{
	ServerScript *script = (ServerScript *)get_script(L);

	if (!script->m_server)
		luaL_error(L, "no server");

	auto players = script->m_server->getPlayersNoLock(script->m_world);
	lua_createtable(L, players.size(), 0);
	size_t i = 0;
	for (Player *p : players) {
		PlayerRef::push(L, p);
		lua_rawseti(L, -2, ++i);
	}
	return 1;
}

