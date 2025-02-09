#pragma once

#include "macros.h"
#include "playerflags.h"
#include "script/scriptevent_fwd.h"
#include "timer.h"
#include "world.h"

// Per-world shared pointer to safely clear and set new world data (swap)
struct WorldMeta : public IWorldMeta {
	WorldMeta(const std::string &id);

	DISABLE_COPY(WorldMeta);

	// Used in Server::pkt_Join / Client::pkt_WorldData
	enum class Type {
		TmpSimple,  // tmp-edit-simple, possibly locked behind code
		TmpDraw,    // tmp-edit-draw,   possibly locked behind code
		Persistent, // owned (all permissions)
		Readonly,   // imported worlds
		MAX_INVALID
	} type = Type::MAX_INVALID;

	static Type idToType(const std::string &id);

	/// Fields to keep on clear
	PlayerFlags getPlayerFlags(const std::string &name) const;
	void setPlayerFlags(const std::string &name, const PlayerFlags pf);
	void changePlayerFlags(const std::string &name, playerflags_t changed, playerflags_t mask);
	const std::map<std::string, PlayerFlags> &getAllPlayerFlags() const { return m_player_flags; }
	// For database
	void readPlayerFlags(Packet &pkt);
	void writePlayerFlags(Packet &pkt) const;

	std::unique_ptr<ScriptEventMap> script_events_to_send;

	// Activated keys
	Timer keys[3] = {};
	std::string edit_code;
	int spawn_index = -1;

	/// Removes the oldest history until nelements is reached
	void trimChatHistory(size_t nelements);

	struct ChatHistory {
		time_t timestamp;
		std::string name;
		std::string message;
	};
	/// Newest messages added to back, oldest removed from front
	std::vector<ChatHistory> chat_history;

private:
	std::map<std::string, PlayerFlags> m_player_flags;
};

