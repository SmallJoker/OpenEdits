#include "script.h"
#include "script_utils.h"
#include "core/blockmanager.h"

using namespace ScriptUtils;

static Logger &logger = script_logger;

// -------------- Static Lua functions -------------

namespace {
	struct BlockID {
		bid_t id;
		//const char *name;
	};
};

static BlockID pull_blockid(lua_State *L, int idx)
{
	int id = -1;
	if (lua_isnumber(L, idx)) {
		id = lua_tonumber(L, idx);
	} else {
		// maybe add string indices later?
		id = check_field_int(L, -1, "id");
	}
	if (id < 0 || id >= Block::ID_INVALID)
		luaL_error(L, "Invalid block ID %d", id);

	BlockID ret;
	ret.id = id;
	return ret;
}

static BlockDrawType read_block_drawtype(lua_State *L, int idx)
{
	int type = luaL_checkint(L, idx);
	if (type < 0 || type >= (int)BlockDrawType::Invalid)
		luaL_error(L, "Invalid BlockDrawType value");
	return (BlockDrawType)type;
}

static TileOverlayType read_tile_overlay_type(lua_State *L, int idx)
{
	int type = luaL_checkint(L, idx);
	if (type < 0 || type >= (int)TileOverlayType::Invalid)
		luaL_error(L, "Invalid TileOverlayType value");
	return (TileOverlayType)type;
}

// -------------- Script class functions -------------

int Script::l_include(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);

	std::string asset_name = luaL_checkstring(L, 1);
	bool is_public = true;
	if (!lua_isnoneornil(L, 2)) {
		std::string scope = luaL_checkstring(L, 2);
		is_public = (scope != "server");
		// TODO: support "client"-only scripts.
	}

	if (!is_public)
		script->m_private_include_depth++;

	bool ok = script->loadFromAsset(asset_name);

	if (!is_public)
		script->m_private_include_depth--;

	if (!ok)
		luaL_error(L, "Failed to load script"); // provides backtrace

	MESSY_CPP_EXCEPTIONS_END
	return 0;
}

int Script::l_load_hardcoded_packs(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);

	script->m_bmgr->doPackRegistration();
	// required media is added by BlockManager::sanityCheck()

	MESSY_CPP_EXCEPTIONS_END
	return 0;
}

int Script::l_register_pack(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);
	script->m_last_block_id = Block::ID_INVALID;

	luaL_checktype(L, 1, LUA_TTABLE);
	const char *name = check_field_string(L, 1, "name");

	auto pack = std::make_unique<BlockPack>(name);
	lua_getfield(L, 1, "blocks");
	{
		int table = lua_gettop(L);
		for (lua_pushnil(L); lua_next(L, table); lua_pop(L, 1)) {
			// key @ -2, value @ -1
			(void)luaL_checkint(L, -2);

			BlockID id = pull_blockid(L, -1);
			// maybe add string indices later?

			pack->block_ids.push_back(id.id);
		}
	}
	lua_pop(L, 1); // blocks

	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "default_type");
	pack->default_type = read_block_drawtype(L, -1);
	lua_pop(L, 1); // table

	logger(LL_INFO, "register pack: %s\n", pack->name.c_str());
	script->m_bmgr->registerPack(pack.release());

	MESSY_CPP_EXCEPTIONS_END
	return 0;
}

int Script::l_change_block(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Script *script = get_script(L);

	const BlockID id = pull_blockid(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	const bid_t block_id = id.id;
	script->m_last_block_id = block_id;

	BlockProperties *props = script->m_bmgr->getPropsForModification(block_id);
	if (!props)
		return luaL_error(L, "block_id=%i not found", block_id);

	// ---------- Physics


	function_ref_from_field(L, 2, "on_placed",         props->ref_on_placed);
	function_ref_from_field(L, 2, "on_intersect_once", props->ref_intersect_once);
	function_ref_from_field(L, 2, "on_intersect",      props->ref_on_intersect);
	function_ref_from_field(L, 2, "on_collide",        props->ref_on_collide);

	lua_getfield(L, 2, "viscosity");
	if (!lua_isnil(L, -1)) {
		lua_Number viscosity = luaL_checknumber(L, -1);
		props->viscosity = viscosity;
	}
	lua_pop(L, 1);

	// ---------- Audiovisuals

	lua_getfield(L, 2, "minimap_color");
	if (!lua_isnil(L, -1)) {
		lua_Integer minimap_color = luaL_checkinteger(L, -1);
		props->color = minimap_color;
	}
	lua_pop(L, 1);

	lua_getfield(L, 2, "tiles");
	if (!lua_isnil(L, -1)) {
		int table = lua_gettop(L);
		luaL_checktype(L, table, LUA_TTABLE);
		auto &tiles = props->tiles; // setTiles would overwrite everything :(

		// Allow selective changes, e.g. [2] = { type = ..., alpha = ... }
		lua_pushnil(L);
		while (lua_next(L, table)) {
			// key @ -2, value @ -1?
			luaL_checktype(L, -1, LUA_TTABLE);
			size_t i = luaL_checkint(L, -2) - 1;
			if (i >= tiles.size())
				tiles.resize(i + 1);

			BlockTile &tile = tiles.at(i);
			tile.is_known_tile = true;

			{
				lua_getfield(L, -1, "type");
				if (!lua_isnil(L, -1))
					tile.type = read_block_drawtype(L, -1);
				else if (tile.type == BlockDrawType::Invalid)
					tile.type = props->pack->default_type;
				lua_pop(L, 1); // type
			}

			{
				lua_getfield(L, -1, "alpha");
				if (!lua_isnil(L, -1))
					tile.have_alpha = lua_toboolean(L, -1);
				lua_pop(L, 1); // alpha
			}

			{
				lua_getfield(L, -1, "override");
				if (!lua_isnil(L, -1)) {
					luaL_checktype(L, -1, LUA_TTABLE);
					tile.visual_override.enabled = true;
					tile.visual_override.id = pull_blockid(L, -1).id;
					tile.visual_override.tile = check_field_int(L, -1, "tile");
				}
				lua_pop(L, 1); // override
			}

			{
				lua_getfield(L, -1, "overlay");
				if (!lua_isnil(L, -1))
					tile.overlay = read_tile_overlay_type(L, -1);
				lua_pop(L, 1); // overlay
			}

			lua_pop(L, 1); // value
		}
	} // else: 1 tile with default type
	lua_pop(L, 1);


	// ---------- GUI

#if BUILD_CLIENT
	function_ref_from_field(L, 2, "get_visuals", props->ref_get_visuals);
	function_ref_from_field(L, 2, "gui_def",     props->ref_gui_def, LUA_TTABLE);
#endif
	if (!BUILD_CLIENT) {
		check_field_type(L, 2, "get_visuals", LUA_TFUNCTION);
		check_field_type(L, 2, "gui_def",     LUA_TTABLE);
	}


	// ---------- Other

	lua_getfield(L, 2, "params");
	if (!lua_isnil(L, -1)) {
		int value = luaL_checkinteger(L, -1);
		if (value < 0 || value > (int)BlockParams::Type::INVALID)
			luaL_error(L, "Invalid params type");
		props->paramtypes = (BlockParams::Type)value;
	}
	lua_pop(L, 1);

	logger(LL_DEBUG, "Changed block_id=%i\n", block_id);

	MESSY_CPP_EXCEPTIONS_END
	return 0;
}
