#ifdef HAVE_LUA

#include "script.h"
#include "blockmanager.h"
#include "connection.h" // protocol version
#include "player.h"

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

static int dump_args(lua_State *L, FILE *file)
{
	int nargs = lua_gettop(L);
	for (int i = 0; i < nargs; ++i) {
		fprintf(file, "%s", lua_tostring(L, i + 1));
		if (i + 1 < nargs)
			fprintf(file, "\t");
	}
}

static int l_print(lua_State *L)
{
	printf("Lua print: ");
	dump_args(L, stdout);
	fflush(stdout);
	return 0;
}

static int l_error(lua_State *L)
{
	fprintf(stderr, "Lua error: ");
	dump_args(L, stderr);
	fflush(stderr);
	return 0;
}

static int l_panic(lua_State *L)
{
	fprintf(stderr, "Lua panic! unprotected error in %s\n", lua_tostring(L, -1));
	return 0;
}

Script::~Script()
{
	if (m_lua) {
		lua_close(m_lua);
		m_lua = nullptr;
	}
}


bool Script::init()
{
	m_lua = lua_open();
	if (!m_lua)
		return false;

	luaopen_base(m_lua);

	// The most important functions
	lua_atpanic(m_lua, l_panic);
	lua_pushcfunction(m_lua, l_print);
	lua_setglobal(m_lua, "print");
	lua_pushcfunction(m_lua, l_error);
	lua_setglobal(m_lua, "error");

	lua_newtable(m_lua);
	{
		lua_pushinteger(m_lua, PROTOCOL_VERSION_MIN);
		lua_setfield(m_lua, -2, "PROTO_VER_MIN");
		lua_pushinteger(m_lua, PROTOCOL_VERSION_MAX);
		lua_setfield(m_lua, -2, "PROTO_VER_MAX");
		/*
			Not populated variables:
				PROTO_VER
				player
				get_handler
		*/
	}
	lua_setglobal(m_lua, "env");

	return true;
}


bool Script::loadFromFile(const std::string &filename)
{
	if (luaL_dofile(m_lua, filename.c_str()) == 0) {
		return true; // good
	}

	const char *err = lua_tostring(m_lua, -1);
	fprintf(stderr, "Lua: failed to load script: %s\n",
		err ? err : "(no error message)");
	lua_pop(m_lua, 1); // error message

	return false;
}

#if 0
#define ARRAYSIZE(ARR) (sizeof(ARR) / sizeof(ARR[0]))

const struct lut_drawtype_t {
	std::string name;
	BlockDrawType value;
} LUT_DRAWTYPE[] = {
	{ "solid",      BlockDrawType::Solid      },
	{ "action",     BlockDrawType::Action     },
	{ "decoration", BlockDrawType::Decoration },
	{ "background", BlockDrawType::Background }
};

static void load_definition(lua_State *L)
{
	lua_getfield(L, -1, "drawtype");
	const char *drawtype_s = luaL_checkstring(L, -1);

	const lut_drawtype_t *drawtype_p = nullptr;
	for (size_t i = 0; i < ARRAYSIZE(LUT_DRAWTYPE); ++i) {
		if (drawtype_s == LUT_DRAWTYPE[i].name) {
			drawtype_p = &LUT_DRAWTYPE[i];
			puts("MATCH");
			break;
		}
	}

	if (!drawtype_p) {
		luaL_error(L, "Unknown drawtype: %s", drawtype_s);
	}
	lua_pop(L, 1);
}
#endif

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
		fprintf(stderr, "Lua: def for block_id=%d failed: %s\n",
			block_id,
			lua_tostring(L, -1)
		);
		lua_pop(L, 1); // pop error
		return false;
	}

	// Process returned table
	luaL_checktype(L, -1, LUA_TTABLE);
	puts("OK");

	{
		lua_getfield(L, -1, "texture");
		printf("texture = %s\n", luaL_checkstring(L, -1));
		lua_pop(L, 1); // field
	}
	{
		lua_getfield(L, -1, "texture_start_index");
		printf("texture_start_index = %ld\n", luaL_checkinteger(L, -1));
		lua_pop(L, 1); // field
	}

	lua_pop(L, nresults); // pop return values
	return true;
}


void Script::whileIntersecting(Player *player)
{
	// replacement for BlockDefinition::step
	// push a reference (table or userdata) to modify
}


#endif
