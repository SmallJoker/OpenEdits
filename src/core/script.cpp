#include "script.h"
#include "blockmanager.h"
#include "connection.h" // protocol version
#include "logger.h"
#include "mediamanager.h"
#include "player.h"
#include <fstream>
#include <memory> // unique_ptr

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

static Logger logger("Script", LL_INFO);

static const lua_Integer SCRIPT_API_VERSION = 1;
enum RIDX_Indices : lua_Integer {
	CUSTOM_RIDX_SCRIPT = 1,
	CUSTOM_RIDX_TRACEBACK,
};

/*
	Sandbox theory: http://lua-users.org/wiki/SandBoxes

	Creating a secure Lua sandbox: https://stackoverflow.com/a/34388499
		Use: setfenv
		Blacklist: getfenv, getmetatable, debug.*
*/

// -------------- Static Lua functions -------------

static void dump_args(lua_State *L, FILE *file, bool details = false)
{
	if (details)
		puts("");

	int nargs = lua_gettop(L);
	char buf[64];
	for (int i = 1; i < nargs + 1; ++i) {
		const char *str = "??";
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TBOOLEAN:
				str = lua_toboolean(L, i) ? "true" : "false";
				break;
			case LUA_TSTRING:
			case LUA_TNUMBER:
				str = lua_tostring(L, i);
				break;
			case LUA_TFUNCTION:
				snprintf(buf, sizeof(buf), "%p", lua_topointer(L, i));
				str = buf;
				break;
			default: break;
		}
		if (details)
			fprintf(file, "\t#%i(%s) = %s\n", i, lua_typename(L,type ), str);
		else
			fprintf(file, "\t%s", str);
	}
	if (!details)
		puts("");
}

static int l_print(lua_State *L)
{
	logger(LL_PRINT, "%s: ", __func__);
	dump_args(L, stdout);
	fflush(stdout);
	return 0;
}

static int l_error(lua_State *L)
{
	logger(LL_ERROR, "%s: ", __func__);
	dump_args(L, stderr, true);
	fflush(stderr);
	return 0;
}

static int l_panic(lua_State *L)
{
	logger(LL_ERROR, "Panic! %s\n", lua_tostring(L, -1));
	return 0;
}

static Script *get_script(lua_State *L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_SCRIPT);
	Script *ret = (Script *)lua_touserdata(L, -1);
	lua_pop(L, 1); // value
	return ret;
}

static const char *check_field_string(lua_State *L, int idx, const char *field)
{
	lua_getfield(L, idx, field);
	const char *ret = luaL_checkstring(L, -1);
	lua_pop(L, 1); // field
	return ret;
}

static lua_Integer check_field_int(lua_State *L, int idx, const char *field)
{
	lua_getfield(L, idx, field);
	lua_Integer ret = luaL_checkinteger(L, -1);
	lua_pop(L, 1); // field
	return ret;
}

/// true on success
static bool function_to_ref(lua_State *L, int &ref)
{
	luaL_checktype(L, -1, LUA_TFUNCTION);
	if (ref >= 0)
		luaL_unref(L, LUA_REGISTRYINDEX, ref);

	ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops value
	return ref >= 0;
}

// -------------- Script class functions -------------

Script::Script(BlockManager *bmgr) :
	m_bmgr(bmgr)
{
	ASSERT_FORCED(m_bmgr, "Missing BlockManager");

	protocol_version = PROTOCOL_VERSION_MAX;
}


Script::~Script()
{
	close();
}

static const std::string G_WHITELIST[] = {
	"_G",
	"assert",
	"pairs",
	"ipairs",
	"next",
	"pcall",
	"xpcall",
	"select",
	"tostring",
	"type",
	"unpack",
};

static void process_api_whitelist(lua_State *L)
{
	lua_getglobal(L, "_G");
	int table = lua_gettop(L);
	lua_pushnil(L);
	while (lua_next(L, table)) {
		// key @ -2, value @ -1
		std::string str = lua_tostring(L, -2);
		bool found = false;
		for (const std::string &white : G_WHITELIST) {
			if (white == str) {
				found = true;
				break;
			}
		}
		if (!found) {
			lua_pushnil(L);
			lua_setfield(L, table, str.c_str());
		}
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

static void field_set_function(lua_State *L, const char *name, lua_CFunction func)
{
	lua_pushcfunction(L, func);
	lua_setfield(L, -2, name);
}


bool Script::init()
{
	m_lua = lua_open();
	if (!m_lua)
		return false;

	lua_State *L = m_lua;
	luaopen_base(L);
	{
		luaopen_debug(L); // debug.traceback (invalidated later)
		lua_getglobal(L, "debug");
		lua_getfield(L, -1, "traceback");
		lua_remove(L, -2); // debug
		lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	}

	// Remove functions that we probably don't need
	process_api_whitelist(L);

	luaopen_math(L);
	if (do_load_string_n_table) {
		// not really needed but helpful for tests
		luaopen_string(L);
		luaopen_table(L);
	}

	// The most important functions
	lua_atpanic(L, l_panic);
	lua_pushcfunction(L, l_print);
	lua_setglobal(L, "print");
	lua_pushcfunction(L, l_error);
	lua_setglobal(L, "error");

	// To retrieve this instance within function calls
	lua_pushlightuserdata(L, this);
	lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_SCRIPT);

#define FIELD_SET_FUNC(prefix, name) \
	field_set_function(L, #name, Script::l_ ## prefix ## name)

	// Environment definition
	lua_newtable(L);
	{
		lua_pushinteger(L, SCRIPT_API_VERSION);
		lua_setfield(L, -2, "API_VERSION");
		FIELD_SET_FUNC(/**/, include);
		FIELD_SET_FUNC(/**/, register_pack);
		FIELD_SET_FUNC(/**/, change_block);
		/*
			Not populated variables:
				PROTO_VER
				player
				get_handler
		*/

		lua_newtable(L);
		{
			FIELD_SET_FUNC(player_, get_pos);
			FIELD_SET_FUNC(player_, set_pos);
			FIELD_SET_FUNC(player_, get_acc);
			FIELD_SET_FUNC(player_, set_acc);
		}
		lua_setfield(L, -2, "player");
	}
	lua_setglobal(L, "env");

#undef FIELD_SET_FUNC
	if (!do_load_string_n_table) {
		lua_pushnil(L);
		lua_setglobal(L, "_G");
	}

	logger(LL_PRINT, "init done\n");
	return true;
}

void Script::close()
{
	if (!m_lua)
		return;

	logger(LL_PRINT, "closing ...");

	auto &list = m_bmgr->getPropsForModification();
	// Unregister any callbacks. `lua_close` will clean up the references
	for (BlockProperties *props : list) {
		if (!props)
			continue;
		props->ref_on_intersect = LUA_REFNIL;
		props->ref_on_collide = LUA_REFNIL;
	}

	lua_close(m_lua);
	m_lua = nullptr;
}

bool Script::loadFromAsset(const std::string &asset_name)
{
	ASSERT_FORCED(m_media, "Missing MediaManager");

	const char *real_path = m_media->getAssetPath(asset_name.c_str());
	if (!real_path)
		return false;

	if (asset_name.find_first_of(":/\\") != std::string::npos) {
		logger(LL_ERROR, "Invalid asset name '%s'", asset_name.c_str());
		return false;
	}

	if (!loadFromFile(real_path))
		return false;

	return m_media->requireAsset(asset_name.c_str());
}

static const char *SEPARATOR = "----------------\n";

bool Script::loadFromFile(const std::string &filename)
{
	int first_char = 0;
	{
		std::ifstream file(filename, std::ios_base::binary);
		first_char = file.get();
	}

	if (first_char == 27) {
		logger(LL_ERROR, "Loading bytecode is not allowed\n");
		return false;
	}

	logger(LL_INFO, "Loading file: '%s'\n", filename.c_str());
	lua_rawgeti(m_lua, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	luaL_checktype(m_lua, -1, LUA_TFUNCTION);
	int errorhandler = lua_gettop(m_lua);

	int status = luaL_loadfile(m_lua, filename.c_str());
	if (status == 0)
		status = lua_pcall(m_lua, 0, LUA_MULTRET, errorhandler);

	if (status != 0) {
		const char *err = lua_tostring(m_lua, -1);
		logger(LL_ERROR, "Failed to load script '%s' (ret=%d):\n%s%s\n%s",
			filename.c_str(),
			status,
			SEPARATOR,
			err ? err : "(no error message)",
			SEPARATOR
		);

		lua_pop(m_lua, 1); // error message
	}
	lua_pop(m_lua, 1); // error handler

	return status == 0;
}

bool Script::loadDefinition(bid_t block_id)
{
	lua_State *L = m_lua;

	// Function call prepration
	lua_getglobal(L, "env");
	luaL_checktype(L, -1, LUA_TTABLE);
	lua_getfield(L, -1, "get_definition");
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushinteger(L, block_id);             // #1
	lua_pushinteger(L, protocol_version); // #2

	// Execute!
	const int nresults = 1; // table
	if (lua_pcall(L, 2, nresults, 0)) {
		logger(LL_ERROR, ": def for block_id=%d failed: %s\n",
			block_id,
			lua_tostring(L, -1)
		);
		lua_pop(L, 1); // pop error
		return false;
	}

	// Process returned table
	luaL_checktype(L, -1, LUA_TTABLE);

	//readDefinition();

	lua_pop(L, nresults); // pop return values
	return true;
}

void Script::setTestMode(const std::string &value)
{
	lua_State *L = m_lua;

	lua_getglobal(L, "env");
	lua_pushlstring(L, value.c_str(), value.size());
	lua_setfield(L, -2, "test_mode");
	lua_pop(L, 1); // env
}

int Script::popErrorCount()
{
	return logger.popErrorCount();
}


// -------------- Callback functions -------------

bool Script::haveOnIntersect(const BlockProperties *props) const
{
	return props && props->ref_on_intersect >= 0;
}

void Script::onIntersect(const BlockProperties *props)
{
	lua_State *L = m_lua;

	if (!haveOnIntersect(props)) {
		// no callback registered: fall-back to air
		props = m_bmgr->getProps(0);
	}

	int top = lua_gettop(L);
	// Function call prepration
	// This is faster than calling a getter function
	lua_rawgeti(L, LUA_REGISTRYINDEX, props->ref_on_intersect);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	// Execute!
	if (lua_pcall(L, 0, 0, 0)) {
		logger(LL_ERROR, "on_intersect pack='%s' failed: %s\n",
			props->pack->name.c_str(),
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}


bool Script::haveOnCollide(const BlockProperties *props) const
{
	return props && props->ref_on_collide >= 0;
}

int Script::onCollide(CollisionInfo ci)
{
	lua_State *L = m_lua;
	using CT = BlockProperties::CollisionType;

	if (!haveOnCollide(ci.props)) {
		ASSERT_FORCED(ci.props->tiles[0].type != BlockDrawType::Solid,
			"Should not be called.");
		return (int)CT::None; // should not be returned: incorrect on solids
	}

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ci.props->ref_on_collide);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushinteger(L, ci.pos.X);
	lua_pushinteger(L, ci.pos.Y);
	lua_pushboolean(L, ci.is_x);
	// Execute!
	if (lua_pcall(L, 3, 1, 0)) {
		logger(LL_ERROR, "on_collide pack='%s' failed: %s\n",
			ci.props->pack->name.c_str(),
			lua_tostring(L, -1)
		);
		lua_settop(L, top);
		return (int)CT::None;
	}

	int collision_type = (int)CT::None;
	if (lua_isnumber(L, -1))
		collision_type = lua_tonumber(L, -1);
	else
		collision_type = -1; // invalid

	switch (collision_type) {
		case (int)CT::None:
		case (int)CT::Velocity:
		case (int)CT::Position:
			// good
			logger(LL_DEBUG, "collision_type=%i, pos=(%i,%i), dir=%s\n",
				collision_type, ci.pos.X, ci.pos.Y, ci.is_x ? "X" : "Y"
			);
			break;
		default:
			collision_type = 0;
			logger(LL_ERROR, "invalid collision in pack='%s'\n",
				ci.props->pack->name.c_str()
			);
			break;
	}
	lua_settop(L, top);

	return collision_type;
}

// -------------- Static Script class functions -------------

static char tmp_errorlog[100];
static void cpp_exception_handler(lua_State *L)
{
	int n = -1;
	try {
		throw;
	} catch (std::runtime_error &e) {
		n = snprintf(tmp_errorlog, sizeof(tmp_errorlog), "std::runtime_error(\"%s\")", e.what());
	} catch (std::exception &e) {
		n = snprintf(tmp_errorlog, sizeof(tmp_errorlog), "std::exception(\"%s\")", e.what());
	}
	// Cannot catch lua_longjmp: incomplete type. RAII might not work.

	if (n > 0) {
		luaL_error(L, tmp_errorlog);
	} else {
		logger(LL_ERROR, "C++ exception handler failed\n");
		lua_error(L);
	}
}

#define MESSY_CPP_EXCEPTIONS(my_code) \
	try { \
		logger(LL_DEBUG, "-> call %s\n", __func__); \
		my_code \
	} catch (...) { \
		cpp_exception_handler(L); \
		return 0; \
	}

int Script::l_include(lua_State *L)
{MESSY_CPP_EXCEPTIONS(

	Script *script = get_script(L);
	std::string asset_name = luaL_checkstring(L, 1);

	if (!script->loadFromAsset(asset_name))
		lua_error(L);

	return 0;
)}

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

int Script::l_register_pack(lua_State *L)
{MESSY_CPP_EXCEPTIONS(
	Script *script = get_script(L);

	luaL_checktype(L, 1, LUA_TTABLE);
	const char *name = check_field_string(L, 1, "name");

	auto pack = std::make_unique<BlockPack>(name);

	lua_getfield(L, 1, "blocks");
	{
		int table = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, table)){
			// key @ -2, value @ -1
			luaL_checkint(L, -2);

			BlockID id = pull_blockid(L, -1);
			// maybe add string indices later?

			pack->block_ids.push_back(id.id);
			lua_pop(L, 1); // value
		}
	}
	lua_pop(L, 1); // table


	lua_getfield(L, 1, "default_type");
	{
		int type = luaL_checkint(L, -1);
		if (type < 0 || type >= (int)BlockDrawType::Invalid)
			luaL_error(L, "Unknown default_type value");

		pack->default_type = (BlockDrawType)type;
	}
	lua_pop(L, 1); // table

	logger(LL_INFO, "register pack: %s\n", pack->name.c_str());
	script->m_bmgr->registerPack(pack.release());
	return 0;
)}

int Script::l_change_block(lua_State *L)
{MESSY_CPP_EXCEPTIONS(
	Script *script = get_script(L);

	const BlockID id = pull_blockid(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	const bid_t block_id = id.id;

	BlockProperties *props = script->m_bmgr->getPropsForModification(block_id);
	if (!props)
		return luaL_error(L, "block_id=%i not found", block_id);

	lua_getfield(L, 2, "on_intersect");
	if (!lua_isnil(L, -1)) {
		bool ok = function_to_ref(L, props->ref_on_intersect);
		if (!ok) {
			logger(LL_ERROR, "%s ref failed: block_id= %i\n", lua_tostring(L, -2), block_id);
		}
	} else {
		lua_pop(L, 1);
	}

	lua_getfield(L, 2, "on_collide");
	if (!lua_isnil(L, -1)) {
		bool ok = function_to_ref(L, props->ref_on_collide);
		if (!ok) {
			logger(LL_ERROR, "%s ref failed: block_id= %i\n", lua_tostring(L, -2), block_id);
		}
	} else {
		lua_pop(L, 1);
	}

	lua_getfield(L, 2, "minimap_color");
	if (!lua_isnil(L, -1)) {
		lua_Integer minimap_color = luaL_checkinteger(L, -1);
		props->color = minimap_color;
	}
	lua_pop(L, 1);

	lua_getfield(L, 2, "viscosity");
	if (!lua_isnil(L, -1)) {
		lua_Number viscosity = luaL_checknumber(L, -1);
		props->viscosity = viscosity;
	}
	lua_pop(L, 1);

	logger(LL_DEBUG, "Changed block_id=%i\n", block_id);
	return 0;
)}

static void push_v2f(lua_State *L, core::vector2df vec)
{
	lua_pushnumber(L, vec.X);
	lua_pushnumber(L, vec.Y);
}

static void pull_v2f(lua_State *L, int idx, core::vector2df &vec)
{
	if (!lua_isnil(L, idx))
		vec.X = luaL_checknumber(L, idx);
	if (!lua_isnil(L, idx + 1))
		vec.Y = luaL_checknumber(L, idx + 1);
}

int Script::l_player_get_pos(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->pos);
	return 2;
}

int Script::l_player_set_pos(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (player)
		pull_v2f(L, 1, player->pos);
	return 0;
}

int Script::l_player_get_acc(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (!player)
		return 0;

	push_v2f(L, player->acc);
	return 2;
}

int Script::l_player_set_acc(lua_State *L)
{
	Player *player = get_script(L)->m_player;
	if (player)
		pull_v2f(L, 1, player->acc);
	return 0;
}
