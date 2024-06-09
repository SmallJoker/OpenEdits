#include "clientscript.h"
#include "core/script/script_utils.h"
#include "core/blockmanager.h"
#include "core/player.h"
#include "core/world.h"

int ClientScript::implWorldSetTile(blockpos_t pos, int tile)
{
	if (!isMe())
		return 0; // no-op

	lua_State *L = m_lua;
	World *world = m_player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	Block *b = world->getBlockPtr(pos);
	if (!b)
		luaL_error(L, "invalid position");

	const BlockProperties *props = m_bmgr->getProps(b->id);
	if (props->tile_dependent_physics) {
		// The server must broadcast this change to all players so they
		// cannot get out of sync (prediction errors)
		// TODO: maybe send a Packet to the server as request?
		luaL_error(L, "not implemented");
		return 0;
	}
	// else: We may do it locally for smooth gameplay experience

	{
		SimpleLock lock(world->mutex);
		b->tile = tile;
		world->was_modified = true;
	}
	return 0;
}
