#include "script.h"
#include "script_utils.h"
#include "scriptevent.h"
#include "core/blockmanager.h"
#include "core/macros.h"
#include "core/mediamanager.h"
#include <fstream>

using namespace ScriptUtils;

Logger script_logger("Script", LL_INFO);
static Logger &logger = script_logger;


static const lua_Integer SCRIPT_API_VERSION = 4;

/*
	Sandbox theory: http://lua-users.org/wiki/SandBoxes

	Creating a secure Lua sandbox: https://stackoverflow.com/a/34388499
		Use: setfenv
		Blacklist: getfenv, getmetatable, debug.*
*/

// -------------- Static Lua functions -------------

static int l_nop(lua_State *L)
{
	return 0;
}

static int l_print(lua_State *L)
{
	logger(LL_PRINT, "%s: ", __func__);
	dump_args(L, stdout, false);
	fflush(stdout);
	return 0;
}

static int l_error(lua_State *L)
{
	lua_Debug ar;
	const char *file = "???";
	int line_num = -1;

	if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "Sl", &ar)) {
		file = ar.source;
		line_num = ar.currentline;
	}

	logger(LL_ERROR, "%s (%s L%d): ", __func__, file, line_num);
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

Script::Script(BlockManager *bmgr, Type type) :
	m_scripttype(type),
	m_bmgr(bmgr)
{
	ASSERT_FORCED(m_bmgr, "Missing BlockManager");
	m_emgr = new ScriptEventManager(this);
}


Script::~Script()
{
	close();
}

static const char *G_WHITELIST[] = {
	"_G",
	"assert",
	"pairs",
	"ipairs",
	"next",
	"pcall",
	"xpcall",
	"select",
	"tonumber",
	"tostring",
	"type",
	"unpack",
// Tables / libraries
	"table",
	"math",
	"string",
	nullptr
};

static const char *STRING_WHITELIST[] = {
	"byte",
	"char",
	"find",
	"format",
	"rep",
	"sub",
	nullptr
};

static void process_api_whitelist_single(lua_State *L, const char *whitelist[])
{
	int table = lua_gettop(L);
	lua_pushnil(L);
	while (lua_next(L, table)) {
		// key @ -2, value @ -1
		std::string str = lua_tostring(L, -2);
		bool found = false;

		for (auto white = whitelist; *white; ++white) {
			if (*white == str) {
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
}
static void process_api_whitelist(lua_State *L)
{
	lua_getglobal(L, "_G");
	process_api_whitelist_single(L, G_WHITELIST);
	lua_pop(L, 1);

	// no filter for "math".

	lua_getglobal(L, "string");
	process_api_whitelist_single(L, STRING_WHITELIST);
	lua_pop(L, 1);

	// no filter for "table".
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

	luaopen_math(L);
	luaopen_string(L);
	luaopen_table(L);

	// Remove functions that we probably don't need
	process_api_whitelist(L);

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

	// Internal tracker of online players => PlayerRef
	lua_newtable(L);
	lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_PLAYER_REFS);

#define FIELD_SET_FUNC(prefix, name) \
	field_set_function(L, #name, Script::l_ ## prefix ## name)

	// Environment definition
	lua_newtable(L);
	{
		lua_pushinteger(L, SCRIPT_API_VERSION);
		lua_setfield(L, -2, "API_VERSION");
		FIELD_SET_FUNC(/**/, include);
		FIELD_SET_FUNC(/**/, require_asset);
		FIELD_SET_FUNC(/**/, load_hardcoded_packs);
		FIELD_SET_FUNC(/**/, register_pack);
		FIELD_SET_FUNC(/**/, change_block);

		if (m_emgr) {
			lua_newtable(L);
			lua_setfield(L, -2, "event_handlers");
		}

		lua_pushcfunction(L, l_nop);
		lua_setfield(L, -2, "on_player_event");

		lua_newtable(L);
		{
			FIELD_SET_FUNC(world_, get_block);
			FIELD_SET_FUNC(world_, get_blocks_in_range);
			FIELD_SET_FUNC(world_, get_params);
			FIELD_SET_FUNC(world_, set_tile);
		}
		lua_setfield(L, -2, "world");
		FIELD_SET_FUNC(/**/, register_event);
		FIELD_SET_FUNC(/**/, send_event);
	}
	lua_setglobal(L, "env");

	initSpecifics();

#undef FIELD_SET_FUNC
	if (hide_global_table) {
		lua_pushnil(L);
		lua_setglobal(L, "_G");
	}

	// luaopen_* each pushes one value to the stack
	lua_settop(L, 0);
	// start with 1 to get usable return values from `Script::callFunction`.
	lua_pushnil(L);

	logger(LL_PRINT, "init done\n");
	return true;
}

void Script::close()
{
	if (!m_lua && !m_emgr)
		return;

	logger(LL_PRINT, "closing ...");

	closeSpecifics();

	auto &list = m_bmgr->getPropsForModification();
	// Unregister any callbacks. `lua_close` will clean up the references
	for (BlockProperties *props : list) {
		if (!props)
			continue;
#if BUILD_CLIENT
		props->ref_get_visuals = LUA_REFNIL;
		props->ref_gui_def = LUA_REFNIL;
#endif
		props->ref_on_placed = LUA_REFNIL;
		props->ref_intersect_once = LUA_REFNIL;
		props->ref_on_intersect = LUA_REFNIL;
		props->ref_on_collide = LUA_REFNIL;
	}

	delete m_emgr;
	m_emgr = nullptr;

	lua_close(m_lua);
	m_lua = nullptr;
}

bool Script::loadFromAsset(const std::string &asset_name)
{
	ASSERT_FORCED(m_media, "Missing MediaManager");

	const char *real_path = m_media->getAssetPath(asset_name.c_str());

	if (asset_name.find_first_of(":/\\") != std::string::npos) {
		logger(LL_ERROR, "Invalid asset name '%s'", asset_name.c_str());
		ASSERT_FORCED(!real_path, "bad indexer!");
		return false;
	}

	const bool is_public = m_private_include_depth == 0;
	if (!real_path)
		return is_public == (m_scripttype == ST_SERVER); // ignore on client side

	if (!is_public && m_scripttype != ST_SERVER) {
		// May be triggered by unittests
		logger(LL_WARN, "Accessed unobtainable asset '%s'", asset_name.c_str());
		return true; // do not load
	}

	if (!loadFromFile(real_path))
		return false;

	// Server only: mark as required
	return !is_public || m_media->requireAsset(asset_name.c_str());
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
	logger(LL_INFO, "Loading file: '%s' (public? %d)\n",
		filename.c_str(), m_private_include_depth == 0);

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

void Script::onScriptsLoaded()
{
	m_emgr->onScriptsLoaded();

	lua_State *L = m_lua;

	lua_getglobal(L, "env");
	function_ref_from_field(L, -1, "on_step", m_ref_on_step);
	function_ref_from_field(L, -1, "on_player_event", m_ref_on_player_event);
	lua_pop(L, 1); // env
}

void Script::setTestMode(const std::string &value)
{
	lua_State *L = m_lua;

	lua_getglobal(L, "env");
	lua_pushlstring(L, value.c_str(), value.size());
	lua_setfield(L, -2, "test_mode");
	lua_pop(L, 1); // env
}

std::string Script::popTestFeedback()
{
	lua_State *L = m_lua;

	lua_getglobal(L, "env");
	lua_getfield(L, -1, "test_feedback");
	std::string out = luaL_checkstring(L, -1);
	lua_pop(L, 1); // value

	lua_pushstring(L, "");
	lua_setfield(L, -2, "test_feedback");
	lua_pop(L, 1); // env

	return out;
}

int Script::popErrorCount()
{
	return logger.popErrorCount();
}

int Script::l_require_asset(lua_State *L)
{
	Script *script = get_script(L);
	const char *asset_name = luaL_checkstring(L, 1);

	if (!script->m_media->requireAsset(asset_name))
		luaL_error(L, "not found");

	return 0;
}

void Script::onStep(double abstime)
{
	if (m_ref_on_step < 0)
		return; // not defined

	lua_State *L = m_lua;
	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_ref_on_step);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushnumber(L, abstime);

	// Execute!
	if (lua_pcall(L, 1, 0, 0)) {
		logger(LL_ERROR, "on_step failed: %s\n",
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}
