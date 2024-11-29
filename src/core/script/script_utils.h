#pragma once

#include "core/logger.h"

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

struct lua_State;
class Script;

extern Logger script_logger;
#define logger_off(...)


// Must be split to properly show the executed line number in gdb
#define MESSY_CPP_EXCEPTIONS_START \
	try { \
		logger(LL_DEBUG, "-> call %s\n", __func__);

#define MESSY_CPP_EXCEPTIONS_END \
	} catch (...) { \
		ScriptUtils::cpp_exception_handler(L); \
		return 0; \
	}

namespace ScriptUtils {
	enum RIDX_Indices : lua_Integer {
		CUSTOM_RIDX_SCRIPT = 1,
		CUSTOM_RIDX_TRACEBACK,
		CUSTOM_RIDX_PLAYER_CONTROLS,
		CUSTOM_RIDX_PLAYER_REFS,
	};

	Script *get_script(lua_State *L);

	void cpp_exception_handler(lua_State *L);

	[[maybe_unused]]
	static void field_set_function(lua_State *L, const char *name, lua_CFunction func)
	{
		lua_pushcfunction(L, func);
		lua_setfield(L, -2, name);
	}

	void function_ref_from_field(lua_State *L, int idx, const char *field,
			int &ref, bool required = false);

	const char *check_field_string(lua_State *L, int idx, const char *field);
	lua_Integer check_field_int(lua_State *L, int idx, const char *field);

	void dump_args(lua_State *L, FILE *file, bool details);

}
