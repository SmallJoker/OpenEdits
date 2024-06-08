#include "script.h"
#include "script_utils.h"
#include "core/player.h"
#include "core/world.h"

using namespace ScriptUtils;

int Script::l_world_get_block(lua_State *L)
{
	Script *script = get_script(L);
	Player *player = script->m_player;

	blockpos_t pos = player->last_pos;
	if (!lua_isnil(L, 1)) {
		// automatic floor
		pos.X = luaL_checknumber(L, 1) + 0.5f;
		pos.Y = luaL_checknumber(L, 2) + 0.5f;
	}

	World *world = player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	Block b;
	if (!world->getBlock(pos, &b))
		luaL_error(L, "invalid position");

	lua_pushinteger(L, b.id);
	lua_pushinteger(L, b.tile);
	lua_pushinteger(L, b.bg);
	return 3;
}

int Script::l_world_set_tile(lua_State *L)
{
	Script *script = get_script(L);
	Player *player = script->m_player;

	if (player != script->m_my_player)
		return 0;

	blockpos_t pos = player->last_pos;
	if (!lua_isnil(L, 1)) {
		// automatic floor
		pos.X = luaL_checknumber(L, 1) + 0.5f;
		pos.Y = luaL_checknumber(L, 2) + 0.5f;
	}
	int tile = luaL_checkinteger(L, 3);

	World *world = player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	Block *b = world->getBlockPtr(pos);
	if (!b)
		luaL_error(L, "invalid position");

	SimpleLock lock(world->mutex);
	b->tile = tile;
	world->was_modified = true;
	return 0;
}

