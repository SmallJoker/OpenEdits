#pragma once

#include "scriptevent_fwd.h"
#include "core/blockparams.h"
#include <irrTypes.h>
#include <memory> // unique_ptr
#include <set>
#include <vector>

typedef uint32_t peer_t; // same as in ENetPeer

using namespace irr;

class Packet;
class Script;
class ScriptEventManager;

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
*/

// Part of `event_id_t`
enum ScriptEventFlags : event_id_t {
	// Attaches the `peer_id` to the event information
	SEF_HAVE_ACTOR = 0x8000,
	// world: normal event
	// player: with peer-id
	//SEF_IS_ATTRIBUTE = 0x4000,
};

struct ScriptEventData {
	peer_t peer_id; // unused for attributes
	std::vector<BlockParams> data;
};

class ScriptEventManager {
public:
	struct EventDef {
		std::vector<BlockParams::Type> types;
	};

private:
	std::map<event_id_t, EventDef> m_event_defs;
	Script *m_script;
	int m_ref_event_handlers = -2; // LUA_NOREF

public:
	ScriptEventManager(Script *script);
	void onScriptsLoaded();

	decltype(m_event_defs) &getDefs() { return m_event_defs; }

	ScriptEvent readEventFromLua(int start_idx) const;
	void runLuaEventCallback(const ScriptEvent &se) const;

	/// @param invocations Remaining allowed Lua invocations (hard limit)
	/// @return True on success
	bool readNextEvent(Packet &pkt, bool with_peer_id, ScriptEvent &se) const;
	/// Does NOT terminate the packet with UINT16_MAX after writing
	size_t writeBatchNT(Packet &pkt, bool with_peer_id, const ScriptEventMap *to_send) const;

private:
	/// Throws an exception on error
	void prepare(event_id_t event_id, ScriptEvent &se) const;
};
