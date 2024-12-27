#include "serverscript.h"
#include "remoteplayer.h"
#include "server.h"
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "core/world.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;


void ServerScript::initSpecifics()
{
	// env space
	lua_State *L = m_lua;

	lua_getglobal(L, "env");
	{
		lua_newtable(L);
		field_set_function(L, "get_players_in_world", ServerScript::l_get_players_in_world);
		lua_setfield(L, -2, "server");
	}
	{
		lua_getfield(L, -1, "world");
		field_set_function(L, "set_block", ServerScript::l_world_set_block);
		lua_pop(L, 1); // world
	}
	lua_pop(L, 1); // env
}

void ServerScript::onScriptsLoaded()
{
	Script::onScriptsLoaded();

	lua_State *L = m_lua;

	PlayerRef::doRegister(L);

	lua_getglobal(L, "env");
	lua_getfield(L, -1, "server");

	function_ref_from_field(L, -1, "on_player_join", m_ref_on_player_join, false);
	function_ref_from_field(L, -1, "on_player_leave", m_ref_on_player_leave, false);
	lua_pop(L, 2); // env + server
}

static int read_blockparams(lua_State *L, int idx, BlockParams params)
{
	using Type = BlockParams::Type;
	switch (params.getType()) {
		case Type::None:
			return 0;
		case Type::STR16:
			{
				size_t len;
				const char *ptr = luaL_checklstring(L, idx, &len);
				params.text->assign(ptr, len);
			}
			return 1;
		case Type::U8:
			params.param_u8 = luaL_checkint(L, idx);
			return 1;
		case Type::U8U8U8:
			params.teleporter.rotation = luaL_checkint(L, idx);
			params.teleporter.id       = luaL_checkint(L, idx + 1);
			params.teleporter.dst_id   = luaL_checkint(L, idx + 2);
			return 3;
		case Type::INVALID:
			break;
		// DO NOT USE default CASE
	}

	luaL_error(L, "unhandled type=%d", params.getType());
	return 0;
}

int ServerScript::l_world_set_block(lua_State *L)
{
	ServerScript *script = (ServerScript *)get_script(L);
	World *world = script->m_world;
	if (!world)
		luaL_error(L, "no world");

	BlockUpdate bu(script->m_bmgr);
	bid_t block_id = luaL_checknumber(L, 1);
	bu.pos.X = luaL_checknumber(L, 2) + 0.5f;
	bu.pos.Y = luaL_checknumber(L, 3) + 0.5f;
	if (block_id == (0 | BlockUpdate::BG_FLAG)) {
		bu.setErase(block_id);
	} else {
		bu.set(block_id);
		read_blockparams(L, 4, bu.params);
	}

	// See also: `Server::pkt_PlaceBlock`
	const Block *block = world->updateBlock(bu);
	if (block)
		world->proc_queue.insert(bu);

	return 0;
}

int ServerScript::implWorldSetTile(PositionRange range, bid_t block_id, int tile)
{
	lua_State *L = m_lua;
	if (!m_world)
		luaL_error(L, "no world");

	// Note: There is no broadcast. Client-side updates must be performed by events.
	bool modified = m_world->setBlockTiles(range, block_id, tile);
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

