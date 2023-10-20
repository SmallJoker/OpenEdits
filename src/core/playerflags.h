#pragma once

#include <cstdint>
#include <string>

class Packet;
typedef uint32_t playerflags_t;

struct PlayerFlags {
	PlayerFlags() : flags(0) {}
	PlayerFlags(playerflags_t pf) : flags(pf)
	{
		repair();
	}

	inline bool check(playerflags_t cf) const
	{
		return (flags & cf) == cf;
	}
	inline void set(playerflags_t nf, playerflags_t mask)
	{
		flags = (flags & ~mask) | nf;
		repair();
	}

	playerflags_t flags;

	// Only valid within the current world
	enum PlayerFlagsEnum : playerflags_t {
		PF_NONE = 0,

		// Temporary world-specific flags
		PF_MASK_TMP  = 0x000000FF,
		PF_MUTED     = 0x00000001,
		PF_EDIT      = 0x00000002,
		PF_EDIT_DRAW = 0x00000004 | PF_EDIT,
		PF_GODMODE   = 0x00000010,

		// Persistent world-wide roles
		PF_MASK_WORLD = 0x0000FF00,
		PF_COLLAB     = 0x00001000,
		PF_COOWNER    = 0x00004000,
		PF_OWNER      = 0x00008000, // there can only be one

		// Persistent server-wide roles
		PF_MASK_SERVER = 0x00FF0000,
		PF_MODERATOR   = 0x00200000,
		PF_ADMIN       = 0x00800000,

		// Information relevant to others
		PF_MASK_SEND_OTHERS = PF_MASK_WORLD | PF_MASK_SERVER,
		// For network transmission: per-player
		PF_MASK_SEND_PLAYER = PF_MASK_SEND_OTHERS | PF_MASK_TMP
	};

	/// Returns the flags that this player is allowed to change on "target"
	playerflags_t mayManipulate(const PlayerFlags target, const playerflags_t mask);
	/// Restores the default flags of the current player depending on their current role
	void repair();

	// Text output & input for chat commands
	std::string toHumanReadable() const;
	uint32_t getColor() const;
	/// Human readable string of all changeable player flags
	static std::string getFlagList();
	/// Input parsing to convert human readable strings into flags
	static bool stringToPlayerFlags(const std::string &input, playerflags_t *out);
};



