#pragma once

#include "core/logger.h"

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

struct lua_State;
class Script;

extern Logger script_logger;


// Must be split to properly show the executed line number in gdb
#define MESSY_CPP_EXCEPTIONS_START \
	try { \
		logger(LL_DEBUG, "-> call %s\n", __func__);

#define MESSY_CPP_EXCEPTIONS_END \
	} catch (...) { \
		cpp_exception_handler(L); \
		return 0; \
	}

namespace ScriptUtils {
	enum RIDX_Indices : lua_Integer {
		CUSTOM_RIDX_SCRIPT = 1,
		CUSTOM_RIDX_TRACEBACK,
		CUSTOM_RIDX_PLAYER_CONTROLS,
	};

	Script *get_script(lua_State *L);

	void cpp_exception_handler(lua_State *L);

	const char *check_field_string(lua_State *L, int idx, const char *field);
	lua_Integer check_field_int(lua_State *L, int idx, const char *field);

	void dump_args(lua_State *L, FILE *file, bool details);

}
