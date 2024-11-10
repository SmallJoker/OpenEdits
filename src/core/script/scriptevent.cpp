#include "scriptevent.h"
#include "script.h"
#include "core/logger.h"
#include "core/packet.h"

extern "C" {
	#include <lauxlib.h>
	#include <lualib.h>
}

static Logger logger("ScriptEvents", LL_INFO);
#define logger_off(...) {}

// -------------- ScriptEvent -------------

ScriptEvent::ScriptEvent(u16 event_id)
	: event_id(event_id)
{
	data = new Packet();
}

ScriptEvent::ScriptEvent(ScriptEvent &&other)
{
	std::swap(event_id, other.event_id);
	std::swap(data, other.data);
}

ScriptEvent::~ScriptEvent()
{
	delete data;
}

// -------------- ScriptEventManager helpers -------------

static int read_tagged_pkt(lua_State *L, int idx, BlockParams::Type type, Packet &pkt)
{
	using Type = BlockParams::Type;
	switch (type) {
		case Type::STR16:
			pkt.writeStr16(luaL_checkstring(L, ++idx));
			return idx;
		case Type::U8:
			pkt.write<u8>(luaL_checkinteger(L, ++idx));
			return idx;
		case Type::U8U8U8:
			pkt.write<u8>(luaL_checkinteger(L, ++idx));
			pkt.write<u8>(luaL_checkinteger(L, ++idx));
			pkt.write<u8>(luaL_checkinteger(L, ++idx));
			return idx;
		case Type::INVALID:
		case Type::None:
			break;
		// DO NOT USE default CASE
	}

	throw std::exception();
}

static int write_tagged_pkt(lua_State *L, BlockParams::Type type, Packet &pkt)
{
	using Type = BlockParams::Type;
	switch (type) {
		case Type::STR16:
			lua_pushstring(L, pkt.readStr16().c_str());
			return 1;
		case Type::U8:
			lua_pushinteger(L, pkt.read<u8>());
			return 1;
		case Type::U8U8U8:
			lua_pushinteger(L, pkt.read<u8>());
			lua_pushinteger(L, pkt.read<u8>());
			lua_pushinteger(L, pkt.read<u8>());
			return 3;
		case Type::INVALID:
		case Type::None:
			break;
		// DO NOT USE default CASE
	}

	throw std::exception();
}

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

void ScriptEventManager::readDefinitionFromLua()
{

}

ScriptEvent ScriptEventManager::readEventFromLua() const
{
	lua_State *L = m_script->getState();

	const u16 event_id = luaL_checkinteger(L, 1);

	auto def_it = m_event_defs.find(event_id);
	if (def_it == m_event_defs.end())
		throw std::runtime_error("unregistered event: " + std::to_string(event_id));

	ScriptEvent se(event_id);

	const auto &def = def_it->second.types;
	auto it = def.begin();
	const int stack_max = lua_gettop(L);
	// read linearly from function arguments
	for (int idx = 1; idx < stack_max; (void)idx /*nop*/, ++it) {
		if (it == def.end())
			goto error;

		logger(LL_DEBUG, "%s: arg=%d, type=%d (%zu / %zu)", __func__, idx,
			(int)*it, se.data->getReadPos(), se.data->size());
		idx = read_tagged_pkt(L, idx, *it, *se.data);
	}

	if (it != def.end()) {
error:
		logger(LL_ERROR, "%s: argument count mismatch. expected=%zu, got >= %zu",
			__func__, def.size(), it - def.begin());
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
	lua_rawgeti(L, -1, se.event_id);
	if (lua_type(L, -1) != LUA_TFUNCTION) {
		logger(LL_ERROR, "Invalid event_handler id=%d", se.event_id);
		lua_settop(L, top);
		return;
	}

	auto def = m_event_defs.find(se.event_id);
	if (def == m_event_defs.end())
		throw std::runtime_error("unregistered event: " + std::to_string(se.event_id));

	int nargs = 0;
	for (const auto type : def->second.types)
		nargs += write_tagged_pkt(L, type, *se.data);

	if (lua_pcall(L, nargs, 0, 0)) {
		logger(LL_ERROR, "event_handler id=%d failed: %s\n",
			se.event_id,
			lua_tostring(L, -1)
		);
		// pop table + function + error msg
		goto restore_stack;
	}

restore_stack:
	lua_settop(L, top);
}

size_t ScriptEventManager::runBatch(Packet &pkt) const
{
	size_t count = 0;
	while (pkt.getRemainingBytes()) {
		u16 event_id = pkt.read<u16>();
		if (event_id == UINT16_MAX)
			break;

		ScriptEvent se(event_id);
		Packet *pkt_ptr = &pkt;
		std::swap(se.data, pkt_ptr);
		runLuaEventCallback(se);
		std::swap(se.data, pkt_ptr);
		count++;
	}
	return count;
}

size_t ScriptEventManager::writeBatch(Packet &pkt, std::set<ScriptEvent> *events_list) const
{
	size_t count = 0;
	for (const auto &event : *events_list) {
		pkt.write(event.event_id);

		const uint8_t *data;
		size_t len = event.data->readRawNoCopy(&data, -1);
		pkt.writeRaw(data, len);
		count++;
	}
	pkt.write<u16>(UINT16_MAX); // terminator

	return count;

}
