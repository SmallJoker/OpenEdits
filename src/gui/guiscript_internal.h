#pragma once

#include "guiscript.h"
#include "core/logger.h"
#include "core/script/script_utils.h"
#include <map>

struct lua_State;
extern Logger guiscript_logger;

template<typename TKEY, typename TCNT>
static TKEY &get_free_slot(lua_State *L, const char *type,
		std::map<int, TKEY> &map, TCNT &counter, int &id)
{
	if (id < 0) {
		// Generate new ID
		TCNT scan_id = counter;
		while (1) {
			scan_id++;
			if (scan_id == counter)
				luaL_error(L, "out of %s IDs", type);

			if (map.find(scan_id) == map.end()) {
				// Free slot
				break;
			}
		}
		counter = scan_id + 1;
		id = scan_id;
	} else {
		auto it = map.find(id);
		if (it == map.end()) {
			guiscript_logger(LL_WARN, "%s: Cannot find %s id=%d", __func__, type, id);
		}
	}

	return map[id]; // creates a new entry
}
