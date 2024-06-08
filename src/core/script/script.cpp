#include "script.h"
#include "script_utils.h"
#include "core/blockmanager.h"
#include "core/macros.h"
#include "core/mediamanager.h"
#include <fstream>
#include <memory> // unique_ptr

using namespace ScriptUtils;

Logger script_logger("Script", LL_INFO);
static Logger &logger = script_logger;


static const lua_Integer SCRIPT_API_VERSION = 2;

/*
	Sandbox theory: http://lua-users.org/wiki/SandBoxes

	Creating a secure Lua sandbox: https://stackoverflow.com/a/34388499
		Use: setfenv
		Blacklist: getfenv, getmetatable, debug.*
*/

// -------------- Static Lua functions -------------


static int l_print(lua_State *L)
{
	logger(LL_PRINT, "%s: ", __func__);
	dump_args(L, stdout, false);
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

// -------------- Script class functions -------------

Script::Script(BlockManager *bmgr) :
	m_bmgr(bmgr)
{
	ASSERT_FORCED(m_bmgr, "Missing BlockManager");
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

	// `lua_ref` uses LUA_REGISTRYINDEX too, so we must reserve slots beforehand.
	lua_newtable(L);
	lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PLAYER_CONTROLS);

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
			FIELD_SET_FUNC(player_, get_vel);
			FIELD_SET_FUNC(player_, set_vel);
			FIELD_SET_FUNC(player_, get_acc);
			FIELD_SET_FUNC(player_, set_acc);
			FIELD_SET_FUNC(player_, get_controls);
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
		props->ref_intersect_once = LUA_REFNIL;
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

	m_last_block_id = Block::ID_INVALID;
	logger(LL_INFO, "Loading file: '%s'\n", filename.c_str());

	lua_rawgeti(m_lua, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	luaL_checktype(m_lua, -1, LUA_TFUNCTION);
	int errorhandler = lua_gettop(m_lua);

	int status = luaL_loadfile(m_lua, filename.c_str());
	if (status == 0)
		status = lua_pcall(m_lua, 0, LUA_MULTRET, errorhandler);

	if (status != 0) {
		const char *err = lua_tostring(m_lua, -1);
		char buf[100];
		buf[0] = '\0';
		if (m_last_block_id != Block::ID_INVALID)
			snprintf(buf, sizeof(buf), "related to block_id=%d ", m_last_block_id);

		logger(LL_ERROR, "Failed to load script '%s' %s(ret=%d):\n%s%s\n%s",
			filename.c_str(),
			buf,
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

