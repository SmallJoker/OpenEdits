#include "serverscript.h"
#include "core/script/script_utils.h"
#include "core/player.h"
#include "core/world.h"

int ServerScript::implWorldSetTile(blockpos_t pos, int tile)
{
	lua_State *L = m_lua;
	World *world = m_player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	// TODO: BlockUpdate can yet not update the tile number!
	luaL_error(L, "not implemented");
	//world->proc_queue.insert();
	return 0;
}
