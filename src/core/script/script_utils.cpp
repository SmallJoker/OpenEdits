#include "script_utils.h"
#include <stdexcept>

static Logger &logger = script_logger;

namespace ScriptUtils {

Script *get_script(lua_State *L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_SCRIPT);
	Script *ret = (Script *)lua_touserdata(L, -1);
	lua_pop(L, 1); // value
	return ret;
}

static char tmp_errorlog[100]; // assume that only one thread faults at a time
void cpp_exception_handler(lua_State *L)
{
	int n = -1;
	try {
		throw;
	} catch (std::runtime_error &e) {
		n = snprintf(tmp_errorlog, sizeof(tmp_errorlog), "std::runtime_error(\"%s\")", e.what());
	} catch (std::exception &e) {
		n = snprintf(tmp_errorlog, sizeof(tmp_errorlog), "(%s)", e.what());
	}
	// Cannot catch lua_longjmp: incomplete type. RAII might not work.

	if (n > 0) {
		luaL_error(L, tmp_errorlog);
	} else {
		logger(LL_ERROR, "C++ exception handler failed\n");
		lua_error(L);
	}
}

void function_ref_from_field(lua_State *L, int idx, const char *field,
		int &ref, int type)
{
	lua_getfield(L, idx, field);

	if (!lua_isnil(L, -1)) {
		luaL_checktype(L, -1, type);
		if (ref >= 0)
			luaL_unref(L, LUA_REGISTRYINDEX, ref);

		ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops value

		bool ok = ref >= 0;
		if (!ok) {
			logger(LL_ERROR, "%s ref failed\n", lua_tostring(L, -2));
		}
		return;
	}
	lua_pop(L, 1);
}

const char *check_field_string(lua_State *L, int idx, const char *field)
{
	lua_getfield(L, idx, field);
	const char *ret = luaL_checkstring(L, -1);
	lua_pop(L, 1); // field
	return ret;
}

lua_Integer check_field_int(lua_State *L, int idx, const char *field)
{
	lua_getfield(L, idx, field);
	lua_Integer ret = luaL_checkinteger(L, -1);
	lua_pop(L, 1); // field
	return ret;
}

void dump_args(lua_State *L, FILE *file, bool details)
{
	int nargs = lua_gettop(L);
	char buf[64];
	for (int i = 1; i < nargs + 1; ++i) {
		const char *str = "??";
		int type = lua_type(L, i);
		switch (type) {
			case LUA_TNIL:
				str = "<nil>";
				break;
			case LUA_TBOOLEAN:
				str = lua_toboolean(L, i) ? "true" : "false";
				break;
			case LUA_TSTRING:
			case LUA_TNUMBER:
				str = lua_tostring(L, i);
				break;
			case LUA_TFUNCTION:
				snprintf(buf, sizeof(buf), "<func %p>", lua_topointer(L, i));
				str = buf;
				break;
			case LUA_TTABLE:
				snprintf(buf, sizeof(buf), "<#table %zu>", lua_objlen(L, i));
				str = buf;
				break;
			case LUA_TUSERDATA:
			case LUA_TLIGHTUSERDATA:
				snprintf(buf, sizeof(buf), "<%s %p>", lua_typename(L, type), lua_touserdata(L, i));
				str = buf;
				break;
		}
		if (details)
			fprintf(file, "\t#%i(%s) = %s\n", i, lua_typename(L, type), str);
		else
			fprintf(file, "\t%s", str);
	}
	if (!details)
		puts("");
}

} // namespace
