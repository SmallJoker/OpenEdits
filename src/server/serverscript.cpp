#include "serverscript.h"
#include "remoteplayer.h"
#include "server.h"
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "core/world.h"
#include "core/worldmeta.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;


void ServerScript::initSpecifics()
{
	// env space
	lua_State *L = m_lua;

	PlayerRef::doRegister(L);
	pushCurrentPlayerRef();

	lua_getglobal(L, "env");
	{
		lua_newtable(L);
		field_set_function(L, "get_players_in_world", ServerScript::l_get_players_in_world);
		lua_setfield(L, -2, "server");
	}
	{
		lua_getfield(L, -1, "world");
		field_set_function(L, "get_id", ServerScript::l_world_get_id);
		field_set_function(L, "select", ServerScript::l_world_select);
		field_set_function(L, "set_block", ServerScript::l_world_set_block);
		lua_pop(L, 1); // world
	}
	lua_pop(L, 1); // env
}

void ServerScript::onScriptsLoaded()
{
	Script::onScriptsLoaded();
}

// -------- World / events

int ServerScript::l_world_get_id(lua_State *L)
{
	ServerScript *script = (ServerScript *)get_script(L);
	const World *world = script->m_world;
	if (world) {
		lua_pushstring(L, world->getMeta().id.c_str());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int ServerScript::l_world_select(lua_State *L)
{
	ServerScript *script = (ServerScript *)get_script(L);
	const char *id = luaL_checkstring(L, 1);

	auto world = script->m_server->getWorldNoLock(id);
	if (world)
		script->setWorld(world.get());

	lua_pushboolean(L, !!world);
	return 1;
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
		bu.setErase(true);
	} else {
		bu.set(block_id);
		script->readBlockParams(4, bu.params);
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

