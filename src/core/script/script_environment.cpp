#include "script.h"
#include "script_utils.h"
#include "core/player.h"
#include "core/world.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

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
			range.minp = player->getCurrentBlockPos();
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

static void read_blockparams(lua_State *L, int &idx, BlockParams &params)
{
	using Type = BlockParams::Type;
	switch (params.getType()) {
		case Type::INVALID:
		case Type::None:
			break;
		case Type::STR16:
			*params.text = luaL_checkstring(L, ++idx);
			return;
		case Type::U8:
			params.param_u8 = luaL_checkinteger(L, ++idx);
			return;
		case Type::U8U8U8:
			params.teleporter.rotation = luaL_checkinteger(L, ++idx);
			params.teleporter.id       = luaL_checkinteger(L, ++idx);
			params.teleporter.dst_id   = luaL_checkinteger(L, ++idx);
			return;
		// DO NOT USE default CASE
	}

	luaL_error(L, "unhandled type=%d at index=%d", params.getType(), idx);
}

static int write_blockparams(lua_State *L, const BlockParams &params)
{
	using Type = BlockParams::Type;
	switch (params.getType()) {
		case Type::None:
			return 0;
			break;
		case Type::STR16:
			lua_pushstring(L, params.text->c_str());
			return 1;
		case Type::U8:
			lua_pushinteger(L, params.param_u8);
			return 1;
		case Type::U8U8U8:
			lua_pushinteger(L, params.teleporter.rotation);
			lua_pushinteger(L, params.teleporter.id);
			lua_pushinteger(L, params.teleporter.dst_id);
			return 3;
		case Type::INVALID:
			break;
		// DO NOT USE default CASE
	}

	luaL_error(L, "unhandled type=%d", params.getType());
	return 0;
}

int Script::l_send_event(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);
	Player *player = script->m_player;

	const u16 event_id = luaL_checkinteger(L, 1);
	std::vector<BlockParams> data;
	data.reserve(lua_gettop(L) / 2);

	int idx = 1;
	const int stack_max = lua_gettop(L);
	while (idx < stack_max) {
		int type = luaL_checkinteger(L, ++idx);
		logger(LL_DEBUG, "read event idx=%d, type=%d", idx, type);
		BlockParams &params = data.emplace_back((BlockParams::Type)type);
		read_blockparams(L, idx, params);
	}

	if (player->event_list) {
		auto [it, is_new] = player->event_list->emplace(event_id);
		std::swap(*it->data, data);
	}
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

void Script::onEvent(const ScriptEvent &se)
{
	if (m_ref_event_handlers < 0)
		throw std::runtime_error("missing handler");

	lua_State *L = m_lua;

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_ref_event_handlers);
	lua_rawgeti(L, -1, se.event_id);
	if (lua_type(L, -1) != LUA_TFUNCTION) {
		logger(LL_ERROR, "Invalid event_handler id=%d", se.event_id);
		return;
	}

	int nargs = 0;
	for (const BlockParams &params : *se.data)
		nargs += write_blockparams(L, params);

	if (lua_pcall(L, nargs, 0, 0)) {
		logger(LL_ERROR, "event_handler id=%d failed: %s\n",
			se.event_id,
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // table + function + error msg
		return;
	}

	lua_settop(L, top);
}

int Script::l_world_get_block(lua_State *L)
{
	Script *script = get_script(L);
	Player *player = script->m_player;

	blockpos_t pos;
	if (!lua_isnil(L, 1)) {
		// automatic floor
		pos.X = luaL_checknumber(L, 1) + 0.5f;
		pos.Y = luaL_checknumber(L, 2) + 0.5f;
	} else {
		pos = player->getCurrentBlockPos();
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

int Script::l_world_get_params(lua_State *L)
{
	Script *script = get_script(L);
	Player *player = script->m_player;

	blockpos_t pos;
	pos.X = luaL_checknumber(L, 1) + 0.5f;
	pos.Y = luaL_checknumber(L, 2) + 0.5f;

	World *world = player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	const BlockParams *params = world->getParamsPtr(pos);
	if (!params)
		return 0;

	return write_blockparams(L, *params);
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
