#include "scriptevent.h"
#include "script.h"
#include "core/logger.h"
#include "core/packet.h"

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

static Logger logger("ScriptEvent", LL_INFO);


// -------------- ScriptEventManager impl -------------

ScriptEventManager::ScriptEventManager(Script *script) :
	m_script(script)
{
	// ?
}

void ScriptEventManager::onScriptsLoaded()
{
	lua_State *L = m_script->getState();

	lua_getglobal(L, "env");
	{
		// Same as `function_ref_from_field`
		lua_getfield(L, -1, "event_handlers");
		luaL_checktype(L, -1, LUA_TTABLE);

		int &ref = m_ref_event_handlers;
		if (ref >= 0)
			luaL_unref(L, LUA_REGISTRYINDEX, ref);

		ref = luaL_ref(L, LUA_REGISTRYINDEX); // pops value;
		if (ref < 0) {
			logger(LL_ERROR, "%s ref failed\n", lua_tostring(L, -2));
		}
	}
	lua_pop(L, 1); // env
}

ScriptEvent ScriptEventManager::readEventFromLua(int start_idx) const
{
	lua_State *L = m_script->getState();

	const u16 event_id = luaL_checkinteger(L, start_idx);

	ScriptEvent se;
	prepare(event_id, se);

	auto it = se.second.data.begin();
	const int stack_max = lua_gettop(L);
	// read linearly from function arguments
	for (int idx = start_idx; idx < stack_max; (void)idx /*nop*/, ++it) {
		if (it == se.second.data.end())
			goto error;

		logger(LL_DEBUG, "%s: idx=%d, top_t=%d, want_t=%d",
			__func__, idx + 1, lua_type(L, idx + 1), (int)it->getType());
		idx += Script::readBlockParams(L, idx + 1, *it);
	}

	if (it != se.second.data.end()) {
error:
		logger(LL_ERROR, "%s: argument count mismatch. expected=%zu, got=%d",
			__func__, se.second.data.size(), (stack_max - start_idx));
		throw std::exception();
	}

	return se;
}

void ScriptEventManager::runLuaEventCallback(const ScriptEvent &se) const
{
	if (m_ref_event_handlers < 0)
		throw std::runtime_error("missing handler");

	lua_State *L = m_script->getState();

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_ref_event_handlers);
	lua_rawgeti(L, -1, se.first);
	if (lua_type(L, -1) != LUA_TFUNCTION) {
		logger(LL_ERROR, "Invalid event_handler id=%d", se.first);
		lua_settop(L, top);
		return;
	}

	auto def = m_event_defs.find(se.first);
	if (def == m_event_defs.end())
		throw std::runtime_error("unregistered event: " + std::to_string(se.first));

	int nargs = 0;
	for (auto &bp : se.second.data)
		nargs += Script::writeBlockParams(L, bp);

	if (lua_pcall(L, nargs, 0, 0)) {
		logger(LL_ERROR, "event_handler id=%d failed: %s\n",
			se.first,
			lua_tostring(L, -1)
		);
		// pop table + function + error msg
		goto restore_stack;
	}

restore_stack:
	lua_settop(L, top);
}

bool ScriptEventManager::readNextEvent(Packet &pkt, bool with_peer_id, ScriptEvent &se) const
{
	if (pkt.getRemainingBytes() == 0)
		return false;

	u16 event_id = pkt.read<u16>();
	if (event_id == UINT16_MAX)
		return false;

	prepare(event_id, se);

	if (with_peer_id && (se.first & SEF_HAVE_ACTOR))
		se.second.peer_id = pkt.read<peer_t>();

	for (BlockParams &bp : se.second.data)
		bp.read(pkt);

	return true;
}

size_t ScriptEventManager::writeBatchNT(Packet &pkt, bool with_peer_id, const ScriptEventMap *to_send) const
{
	if (!to_send)
		return 0;

	for (auto &se : *to_send) {
		pkt.write(se.first);

		if (with_peer_id && (se.first & SEF_HAVE_ACTOR))
			pkt.write<peer_t>(se.second.peer_id);

		for (const BlockParams &bp : se.second.data)
			bp.write(pkt);
	}
	return to_send->size();
}

void ScriptEventManager::prepare(event_id_t event_id, ScriptEvent &se) const
{
	auto def_it = m_event_defs.find(event_id);
	if (def_it == m_event_defs.end())
		throw std::runtime_error("unregistered event: " + std::to_string(event_id));

	const auto &def = def_it->second.types;
	se.second.data.clear();
	se.second.data.reserve(def.size());
	for (BlockParams::Type type : def)
		se.second.data.emplace_back(type);

	se.first = event_id;
}
