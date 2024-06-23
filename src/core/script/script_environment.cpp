#include "script.h"
#include "script_utils.h"
#include "core/player.h"
#include "core/world.h"

using namespace ScriptUtils;

void Script::get_position_range(lua_State *L, int idx, PositionRange &range)
{
	using PRT = PositionRange::Type;

	int type = PRT::T_CURRENT_POS;
	if (!lua_isnil(L, idx)) {
		type = luaL_checkinteger(L, idx);
		if (type < 0 || type >= PRT::T_MAX_INVALID)
			luaL_error(L, "PRT out of range");
	}

	Script *script = get_script(L);
	Player *player = script->m_player;

	auto read_pos = [&] (blockpos_t size, int iidx, blockpos_t *pos) {
		float x = std::max<float>(0, luaL_checknumber(L, iidx + 0) + 0.5f);
		float y = std::max<float>(0, luaL_checknumber(L, iidx + 1) + 0.5f);

		pos->X = std::min<u16>(x, size.X); // floor
		pos->Y = std::min<u16>(y, size.Y);
	};

	range.type = (PRT)type;
	switch (range.type) {
		case PRT::T_CURRENT_POS:
			range.minp = player->last_pos;
			break;
		case PRT::T_AREA: {
			World *world = player->getWorld().get();
			read_pos(world->getSize(), idx + 1, &range.minp);
			read_pos(world->getSize(), idx + 3, &range.maxp);
		} break;
		case PRT::T_CIRCLE: {
			World *world = player->getWorld().get();
			read_pos(world->getSize(), idx + 1, &range.minp);
			range.radius = luaL_checknumber(L, idx + 3);
		} break;
		case PRT::T_ENTIRE_WORLD:
			break;
		case PRT::T_MAX_INVALID: return;
	}
}


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

	int block_id = luaL_checkinteger(L, 1);
	if (block_id < 0 || block_id > Block::ID_INVALID)
		luaL_error(L, "block_id out of range");

	int tile = luaL_checkinteger(L, 2);
	if (tile < 0 || tile > Block::TILES_MAX)
		luaL_error(L, "tile out of range");

	PositionRange range;
	script->get_position_range(L, 3, range);

	return script->implWorldSetTile(range, block_id, tile);
}

