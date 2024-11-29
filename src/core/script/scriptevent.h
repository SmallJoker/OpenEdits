#pragma once

#include "core/blockparams.h"
#include <irrTypes.h>
#include <map>
#include <set>
#include <vector>

using namespace irr;

class Packet;
class Script;


/*
Local:
	1. New event created by Lua, primitive values from stack: [arg1, arg2, ...]
	2. Check against the blueprint format (BlockParams::Type array)
	3. Add to the batch
	... (add more?)
	4. Send batch as `Packet` to the peer (server / client)

Peer:
	5. Receive `Packet` (batch)
	6. Execute the Lua event handler for each event of the batch

TODO: Event deduplication?
*/

struct ScriptEvent {
	ScriptEvent(u16 event_id);
	ScriptEvent(ScriptEvent &&other);
	~ScriptEvent();

	ScriptEvent &operator=(const ScriptEvent &other) = delete;

	// For std::set
	bool operator<(const ScriptEvent &rhs) const
	{
		return event_id < rhs.event_id && data < rhs.data;
	}

	u16 event_id;
	// Pointer is stupid but I want to keep headers lightweight.
	Packet *data = nullptr;
};

using ScriptEventList = std::set<ScriptEvent>;

class ScriptEventManager {
public:
	struct EventDef {
		std::vector<BlockParams::Type> types;
	};

private:
	std::map<u16, EventDef> m_event_defs;
	Script *m_script;
	int m_ref_event_handlers = -2; // LUA_NOREF

public:
	ScriptEventManager(Script *script);
	void onScriptsLoaded();

	void readDefinitionFromLua();
	decltype(m_event_defs) &getDefs() { return m_event_defs; }

	ScriptEvent readEventFromLua(int start_idx) const;
	void runLuaEventCallback(const ScriptEvent &se) const;

	/// @param invocations Remaining allowed Lua invocations (hard limit)
	/// @return True if all ScriptEvents could be processed.
	bool runBatch(Packet &pkt, size_t &invocations) const;
	size_t writeBatch(Packet &pkt, ScriptEventList *events_list) const;
};
