#ifdef HAVE_LUA

#include "script.h"
#include "blockmanager.h"
#include "connection.h" // protocol version
#include "player.h"
#include <memory> // unique_ptr

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

#if 1
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif
#define ERRORLOG(...) fprintf(stderr, __VA_ARGS__)

static const lua_Integer SCRIPT_API_VERSION = 1;
static const lua_Integer CUSTOM_RIDX_SCRIPT = 1;

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
	fprintf(stdout, "Lua print:");
	dump_args(L, stdout);
	fflush(stdout);
	return 0;
}

static int l_error(lua_State *L)
{
	fprintf(stderr, "Lua error: ");
	dump_args(L, stderr, true);
	fflush(stderr);
	return 0;
}

static int l_panic(lua_State *L)
{
	ERRORLOG("Lua panic! unprotected error in %s\n", lua_tostring(L, -1));
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
		FIELD_SET_FUNC(/**/, register_pack);
		FIELD_SET_FUNC(/**/, register_block);
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
		}
		lua_setfield(L, -2, "player");
	}
	lua_setglobal(L, "env");

#undef FIELD_SET_FUNC
	puts("--> Lua start");

	return true;
}

void Script::close()
{
	if (!m_lua)
		return;

	puts("<-- Lua stop");

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


bool Script::loadFromFile(const std::string &filename)
{
	if (luaL_dofile(m_lua, filename.c_str()) == 0) {
		return true; // good
	}

	const char *err = lua_tostring(m_lua, -1);
	ERRORLOG("Lua: failed to load script: %s\n",
		err ? err : "(no error message)");
	lua_pop(m_lua, 1); // error message

	return false;
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
	lua_pushinteger(L, PROTOCOL_VERSION_MAX); // #2

	// Execute!
	const int nresults = 1; // table
	if (lua_pcall(L, 2, nresults, 0)) {
		ERRORLOG("Lua: def for block_id=%d failed: %s\n",
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

void Script::onIntersect(const BlockProperties *props)
{
	lua_State *L = m_lua;

	if (!props || props->ref_on_intersect < 0) {
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
		ERRORLOG("Lua: on_intersect pack='%s' failed: %s\n",
			props->pack->name.c_str(),
			lua_tostring(L, -1)
		);
		lua_settop(L, top); // function + error msg
		return;
	}

	lua_settop(L, top);
}

int Script::onCollide(CollisionInfo ci)
{
	lua_State *L = m_lua;
	using BPCT = BlockProperties::CollisionType;
	int collision_type = (int)BPCT::None;

	if (!ci.props || ci.props->ref_on_collide < 0)
		return collision_type;

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ci.props->ref_on_collide);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	lua_pushinteger(L, ci.pos.X);
	lua_pushinteger(L, ci.pos.Y);
	lua_pushboolean(L, ci.is_x);
	// Execute!
	if (lua_pcall(L, 3, 1, 0)) {
		ERRORLOG("Lua: on_collide pack='%s' failed: %s\n",
			ci.props->pack->name.c_str(),
			lua_tostring(L, -1)
		);
		lua_settop(L, top);
		return collision_type;
	}

	if (lua_isnumber(L, -1))
		collision_type = lua_tonumber(L, -1);
	else
		collision_type = -1; // invalid

	switch (collision_type) {
		case (int)BPCT::None:
		case (int)BPCT::Velocity:
		case (int)BPCT::Position:
			// good
			break;
		default:
			collision_type = 0;
			ERRORLOG("Lua: invalid collision in pack='%s'\n",
				ci.props->pack->name.c_str()
			);
			break;
	}
	lua_settop(L, top);

	return collision_type;
}

// -------------- Static Script class functions -------------

// yet unused!
int Script::l_register_pack(lua_State *L)
{
	Script *script = get_script(L);

	const char *name = check_field_string(L, -1, "name");
	auto pack = std::make_unique<BlockPack>(name);

	lua_getfield(L, -1, "blocks");
	{
		int table = lua_gettop(L);
		lua_pushnil(L);
		while (lua_next(L, table)){
			// key @ -2, value @ -1
			int id = check_field_int(L, -1, "id");
			pack->block_ids.push_back(id);
			lua_pop(L, 1); // value
		}
	}
	lua_pop(L, 1); // table

	DEBUGLOG("register pack: %s\n", pack->name.c_str());
	//script->m_bmgr->registerPack(pack.release());
	return 0;
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

int Script::l_register_block(lua_State *L)
{
	Script *script = get_script(L);

	lua_getfield(L, -1, "id");
	bid_t block_id = luaL_checkinteger(L, -1);
	lua_pop(L, 1);

	BlockProperties *props = script->m_bmgr->getPropsForModification(block_id);
	if (!props)
		return luaL_error(L, "block_id=%i not found", block_id);

	lua_getfield(L, -1, "on_intersect");
	if (!lua_isnil(L, -1) || block_id == 0) {
		bool ok = function_to_ref(L, props->ref_on_intersect);
		if (!ok) {
			ERRORLOG("Lua %s ref failed: block_id= %i\n", lua_tostring(L, -2), block_id);
		}
	} else {
		lua_pop(L, 1);
	}

	lua_getfield(L, -1, "on_collide");
	if (!lua_isnil(L, -1) || block_id == 0) {
		bool ok = function_to_ref(L, props->ref_on_collide);
		if (!ok) {
			ERRORLOG("Lua %s ref failed: block_id= %i\n", lua_tostring(L, -2), block_id);
		}
	} else {
		lua_pop(L, 1);
	}

	DEBUGLOG("Lua registered block_id=%i\n", block_id);
	return 0;
}

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

#endif
