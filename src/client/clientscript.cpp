#include "clientscript.h"
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
