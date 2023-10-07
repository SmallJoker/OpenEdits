#pragma once

#include <cstdint>
#include <string>

typedef uint32_t playerflags_t;

struct PlayerFlags {
	PlayerFlags() : flags(0) {}
	PlayerFlags(playerflags_t pf) : flags(pf) {}

	inline bool check(playerflags_t cf) const
	{
		return (flags & cf) == cf;
	}
	inline void set(playerflags_t nf, playerflags_t mask)
	{
		flags = (flags & ~mask) | nf;
	}
	std::string toHumanReadable() const;
	static std::string getFlagList();
	static bool stringToPlayerFlags(const std::string &input, playerflags_t *out);

	playerflags_t flags;

	// Only valid within the current world
	enum PlayerFlagsEnum : playerflags_t {
		// Temporary world-specific flags
		PF_MASK_TMP  = 0x000000FF,
		PF_MUTED     = 0x00000001,
		PF_EDIT      = 0x00000002,
		PF_EDIT_DRAW = 0x00000004 | PF_EDIT,
		PF_GODMODE   = 0x00000010,

		// Persistent world-wide
		PF_MASK_WORLD = 0x0000FF00,
		PF_COLLAB     = 0x00001000 | PF_EDIT_DRAW | PF_GODMODE, // persistent edit & god
		PF_COOWNER    = 0x00004000 | PF_COLLAB, // co-owners
		PF_OWNER      = 0x00008000 | PF_COOWNER, // actual world owner

		// Persistent server-wide
		PF_MASK_SERVER = 0x00FF0000,
		PF_MODERATOR   = 0x00200000 | PF_COOWNER,
		PF_ADMIN       = 0x00800000 | PF_OWNER,

		// Flags allowed to change
		PF_CNG_MASK_COOWNER = PF_MUTED | PF_EDIT_DRAW | PF_GODMODE,
		PF_CNG_MASK_OWNER   = PF_CNG_MASK_COOWNER | PF_COOWNER,

		// For network transmission: per-player
		PF_MASK_SEND_PLAYER = PF_MASK_TMP | PF_MASK_WORLD | PF_MASK_SERVER,
	};
};



