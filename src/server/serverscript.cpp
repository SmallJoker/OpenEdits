#include "serverscript.h"
#include "core/script/script_utils.h"
#include "core/player.h"
#include "core/world.h"

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
