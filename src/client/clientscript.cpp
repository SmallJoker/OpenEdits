#include "clientscript.h"
#include "client.h" // for clearTileCacheFor
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "core/blockmanager.h"
#include "core/world.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

void ClientScript::initSpecifics()
{
	lua_State *L = m_lua;

	PlayerRef::doRegister(L);
	pushCurrentPlayerRef();

	lua_getglobal(L, "env");
	{
		field_set_function(L, "is_me", ClientScript::l_is_me);

		lua_getfield(L, -1, "world");
		{
			field_set_function(L, "update_tiles", ClientScript::l_world_update_tiles);
		}
		lua_pop(L, 1); // world
	}
	lua_pop(L, 1); // env
}

void ClientScript::closeSpecifics()
{
}

int ClientScript::l_is_me(lua_State *L)
{
	ClientScript *script = (ClientScript *)get_script(L);
	lua_pushboolean(L, script->getCurrentPlayer() == script->m_my_player);
	return 1;
}

int ClientScript::l_world_update_tiles(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	ClientScript *script = (ClientScript *)get_script(L);
	if (script->getCurrentPlayer() != script->m_my_player)
		return 0; // invalid player

	for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1)) {
		// key @ -2, value @ -1
		bid_t block_id = lua_tonumber(L, -1); // downcast
		script->m_client->clearTileCacheFor(block_id);
	}
	MESSY_CPP_EXCEPTIONS_END
	return 0;
}

int ClientScript::implWorldSetTile(PositionRange range, bid_t block_id, int tile)
{
	lua_State *L = m_lua;
	World *world = m_world;
	if (!world)
		luaL_error(L, "no world");

	const BlockProperties *props = m_bmgr->getProps(block_id);
	if (props->tile_dependent_physics && !invoked_by_server) {
		// The server must broadcast this change to all players so they
		// cannot get out of sync (prediction errors)
		luaL_error(L, "Block tile change must be initiated by server");
		return 0;
	}
	// else: We may do it locally for smooth gameplay experience

	bool modified = world->setBlockTiles(range, block_id, tile);
	lua_pushboolean(L, modified);
	return 1;
}

void ClientScript::getVisuals(const BlockProperties *props, uint8_t *tile, const BlockParams &params)
{
	if (!props || !tile)
		return;

	if (props->tile_dependent_physics) {
		// must be updated by the server!
		logger(LL_WARN, "%s: not allowed for id=%d", __func__, props->id);
		return;
	}

	lua_State *L = m_lua;
	m_last_block_id = props->id;

	lua_pushinteger(L, *tile);
	int nargs = 1 + writeBlockParams(m_lua, params);
	int top = callFunction(props->ref_get_visuals, 1, "get_visuals", nargs, true);
	if (!top)
		return;

	if (lua_isnumber(L, -1)) {
		*tile = lua_tonumber(L, -1);
		logger(LL_DEBUG, "%s: id=%d -> tile=%d\n", __func__, props->id, *tile);
	}

	lua_settop(L, top);
	return;
}

