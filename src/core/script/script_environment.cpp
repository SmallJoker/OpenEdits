#include "script.h"
#include "script_utils.h"
#include "scriptevent.h"
#include "core/packet.h"
#include "core/player.h"
#include "core/world.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

void Script::get_position_range(lua_State *L, int idx, PositionRange &range)
{
	using O = PositionRange::Operator;
	using T = PositionRange::Type;

	int combined = luaL_checkinteger(L, idx);
	int type = combined & T::PRT_MASK;
	int op   = combined & O::PROP_MASK;
	if (combined < 0 || type >= T::PRT_MAX_INVALID)
		luaL_error(L, "PRT out of range");
	if (op >= O::PROP_MAX_INVALID)
		luaL_error(L, "PROP out of range");

	Script *script = get_script(L);
	const Player *player = script->getCurrentPlayer();

	auto read_pos = [&] (blockpos_t size, int iidx, blockpos_t *pos) {
		float x = std::max<float>(0, luaL_checknumber(L, iidx + 0) + 0.5f);
		float y = std::max<float>(0, luaL_checknumber(L, iidx + 1) + 0.5f);

		pos->X = std::min<u16>(x, size.X); // floor
		pos->Y = std::min<u16>(y, size.Y);
	};

	range.op   = (O)op;
	range.type = (T)type;
	switch (range.type) {
		case T::PRT_ONE_BLOCK:
			range.minp.X = luaL_checknumber(L, idx + 1) + 0.5f;
			range.minp.Y = luaL_checknumber(L, idx + 2) + 0.5f;
			break;
		case T::PRT_AREA: {
			World *world = player->getWorld().get();
			read_pos(world->getSize(), idx + 1, &range.minp);
			read_pos(world->getSize(), idx + 3, &range.maxp);
		} break;
		case T::PRT_CIRCLE: {
			World *world = player->getWorld().get();
			read_pos(world->getSize(), idx + 1, &range.minp);
			range.radius = luaL_checknumber(L, idx + 3);
		} break;
		case T::PRT_ENTIRE_WORLD:
			break;
		case T::PRT_MAX_INVALID:
		case T::PRT_MASK:
			// should not occur
			return;
	}
}

int Script::readBlockParams(int idx, BlockParams &params)
{
	lua_State *L = m_lua;

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

static int write_blockparams(lua_State *L, const BlockParams &params)
{
	using Type = BlockParams::Type;
	switch (params.getType()) {
		case Type::None:
			return 0;
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

int Script::l_register_event(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);

	int event_id_raw = luaL_checkinteger(L, 1);
	uint16_t event_id = (uint16_t)event_id_raw;
	if (event_id_raw != event_id || event_id_raw == UINT16_MAX)
		luaL_error(L, "event id=%d is out of range", event_id);

	(void)luaL_checkinteger(L, 2); // flags (TODO)

	auto &defs = script->m_emgr->getDefs();
	auto [it, is_new] = defs.insert({ event_id, {} });
	auto &def = it->second;
	if (!is_new) {
		logger(LL_WARN, "%s: overwriting id=%d", __func__, event_id);
		def = ScriptEventManager::EventDef();
	}

	const int stack_max = lua_gettop(L);
	for (int idx = 2; idx < stack_max; /*nop*/) {
		int type = luaL_checkinteger(L, ++idx);

		using Type = BlockParams::Type;
		switch ((Type)type) {
			case Type::STR16:
				goto good;
			case Type::U8:
				goto good;
			case Type::U8U8U8:
				goto good;
			case Type::None:
			case Type::INVALID:
				break;
			// DO NOT USE default CASE
		}

		throw std::exception();
	good:
		logger_off(LL_DEBUG, "%s: id=%d, type=%d", __func__, event_id, type);
		def.types.push_back((Type)type);
	}

	logger(LL_INFO, "%s: id=%d, cnt=%zu", __func__, event_id, def.types.size());

	// Return the event ID
	lua_pushinteger(L, event_id);
	return 1;
	MESSY_CPP_EXCEPTIONS_END
}

int Script::l_send_event(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);
	Player *player = script->getCurrentPlayer();

	ScriptEvent ev = script->m_emgr->readEventFromLua(1);
	if (!player->script_events)
		player->script_events.reset(new ScriptEventList());

	player->script_events->emplace(std::move(ev));
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

int Script::l_world_get_block(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);
	Player *player = script->getCurrentPlayer();

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
	MESSY_CPP_EXCEPTIONS_END
	return 3;
}

int Script::l_world_get_blocks_in_range(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);
	World *world = script->m_world;
	if (!world)
		luaL_error(L, "no world");

	luaL_checktype(L, 1, LUA_TTABLE);
	luaL_checktype(L, 2, LUA_TTABLE);

	// Argument 1: Options
	struct {
		bool return_pos;
		bool return_tile;
		bool return_params;
	} opt;

	lua_getfield(L, 1, "return_pos");
	opt.return_pos = !lua_isnil(L, -1) && lua_toboolean(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "return_tile");
	opt.return_tile = !lua_isnil(L, -1) && lua_toboolean(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "return_params");
	opt.return_params = !lua_isnil(L, -1) && lua_toboolean(L, -1);
	lua_pop(L, 1);

	// Argument 2: Block ID whitelist
	std::set<bid_t> bid_whitelist;
	for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1)) {
		// key @ -2, value @ -1
		bid_t block_id = lua_tonumber(L, -1); // downcast
		bid_whitelist.insert(block_id);
	}

	// Argument 3: Range
	PositionRange range;
	script->get_position_range(L, 3, range);

	lua_createtable(L, 100, 0); // guessed average
	const int value_count = 0
		+ 1 // block_Id
		+ 2 * opt.return_pos
		+ 1 * opt.return_tile
		+ 2 * opt.return_params; // guessed average

	int ret_index = 0;
	blockpos_t pos;
	Block b;
	for (bool ok = range.iteratorStart(world, &pos); ok; ok = range.iteratorNext(&pos)) {
		world->getBlock(pos, &b);
		if (bid_whitelist.find(b.id) == bid_whitelist.end())
			continue;

		lua_createtable(L, value_count, 0);
		int n = 1; // b.id
		lua_pushinteger(L, b.id);

		if (opt.return_pos) {
			lua_pushinteger(L, pos.X);
			lua_pushinteger(L, pos.Y);
			n += 2;
		}

		if (opt.return_tile) {
			lua_pushinteger(L, b.tile);
			++n;
		}

		if (opt.return_params) {
			auto params = world->getParamsPtr(pos);
			if (params)
				n += write_blockparams(L, *params);
		}

		// Traverse the stack backwards to insert the pushed values
		while (n --> 0) {
			// insert the topmost value
			lua_rawseti(L,
				-n - 2, // position of the table
				n + 1  // table index
			);
		}

		luaL_checktype(L, -1, LUA_TTABLE); // sanity check
		lua_rawseti(L, -2, ++ret_index);
	}

	MESSY_CPP_EXCEPTIONS_END
	return 1;
}

int Script::l_world_get_params(lua_State *L)
{
	Script *script = get_script(L);
	World *world = script->m_world;

	blockpos_t pos;
	pos.X = luaL_checknumber(L, 1) + 0.5f;
	pos.Y = luaL_checknumber(L, 2) + 0.5f;

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
	int tile     = luaL_checkinteger(L, 2);

	if (block_id < 0 || block_id > Block::ID_INVALID)
		luaL_error(L, "block_id out of range");

	PositionRange range;
	script->get_position_range(L, 3, range);

	return script->implWorldSetTile(range, block_id, tile);
}
