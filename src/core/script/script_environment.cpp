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

int Script::l_register_event(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);

	int event_id = luaL_checkinteger(L, 1);
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
		def.push_back((Type)type);
	}

	logger(LL_INFO, "%s: id=%d, cnt=%zu", __func__, event_id, def.size());
	lua_pushinteger(L, event_id);
	return 1;
	MESSY_CPP_EXCEPTIONS_END
}

int Script::l_send_event(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);
	Player *player = script->m_player;

	ScriptEvent ev = script->m_emgr->readEventFromLua();
	if (player->script_events) {
		player->script_events->emplace(std::move(ev));
	}

	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

int Script::l_world_get_block(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
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
	MESSY_CPP_EXCEPTIONS_END
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
