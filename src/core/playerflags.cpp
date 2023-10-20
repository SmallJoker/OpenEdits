#include "playerflags.h"

using P = PlayerFlags::PlayerFlagsEnum;
static constexpr struct Role {
	const char *name;
	P main;
	playerflags_t default_flags;
	playerflags_t allowed_to_change;
	uint32_t color; // AARRGGBB
} ROLES[] = {
	{
		"Admin",
		P::PF_ADMIN,
		0,
		P::PF_MASK_SERVER | P::PF_MASK_WORLD | P::PF_MASK_TMP,
		0xFFFFFF00 // yellow
	},
	{
		"Moderator",
		P::PF_MODERATOR,
		0,
		P::PF_MASK_WORLD | P::PF_MASK_TMP,
		0xFFFF5500 // orange
	},
	{
		"Owner",
		P::PF_OWNER, // world owner (1 player)
		P::PF_COOWNER | P::PF_COLLAB | P::PF_EDIT_DRAW | P::PF_GODMODE,
		P::PF_COOWNER | P::PF_COLLAB | P::PF_MASK_TMP,
		0xFF77AAFF // baby blue
	},
	{
		"Co-owner",
		P::PF_COOWNER,
		P::PF_COLLAB | P::PF_EDIT_DRAW | P::PF_GODMODE,
		P::PF_COLLAB | P::PF_MASK_TMP,
		0xFF0088EE // light blue
	},
	{
		"Collaborator",
		P::PF_COLLAB,
		P::PF_EDIT_DRAW | P::PF_GODMODE,
		0, // no permission to change flags
		0xFF00EECC // teal
	},
	// termination
	{
		"Normal",
		P::PF_NONE, // normal player
		0,
		0,
		0xFFBBBBBB // grey
	}
};

static Role get_role(const playerflags_t flags)
{
	const Role *r = ROLES;
	for (; r->main; ++r) {
		if (flags & r->main)
			break;
	}
	return *r; // can be the last one too
}

playerflags_t PlayerFlags::mayManipulate(const PlayerFlags target, const playerflags_t mask)
{
	Role r_a = get_role(flags);
	Role r_t = get_role(target.flags);

	// "this" must have more specific bits set than "target"
	if (r_a.allowed_to_change & ~r_t.allowed_to_change & mask)
		return r_a.allowed_to_change & mask;
	return 0; // missing permissions
}

void PlayerFlags::repair()
{
	Role r = get_role(flags);
	flags |= r.default_flags;
}

std::string PlayerFlags::toHumanReadable() const
{
	Role r = get_role(flags);
	std::string out;
	// Check for all temporary flags

	if (r.main) {
		out.append("[Role: ");
		out.append(r.name);
		out.append("] ");
	}

	if (check(PF_MUTED))
		out.append("MUTED ");

	if (!r.main) {
		if (check(PF_EDIT_DRAW))
			out.append("edit-draw ");
		else if (check(PF_EDIT))
			out.append("edit-simple ");
		if (check(PF_GODMODE))
			out.append("godmode ");
	}
	return out;
}

uint32_t PlayerFlags::getColor() const
{
	return get_role(flags).color;
}


static const struct {
	std::string str;
	playerflags_t flags;
} STRING_TO_FLAGS_LUT[] = {
	{ "muted",           PlayerFlags::PF_MUTED },
	{ "edit-simple",     PlayerFlags::PF_EDIT },
	{ "edit-draw",       PlayerFlags::PF_EDIT_DRAW },
	{ "godmode",         PlayerFlags::PF_GODMODE },
	{ "collaborator",    PlayerFlags::PF_COLLAB },
	{ "co-owner",        PlayerFlags::PF_COOWNER },
	{ "owner",           PlayerFlags::PF_OWNER },
};

std::string PlayerFlags::getFlagList()
{
	std::string output;
	for (auto &v : STRING_TO_FLAGS_LUT) {
		if (!output.empty())
			output.append(" ");

		output.append(v.str);
	}

	return output;
}


bool PlayerFlags::stringToPlayerFlags(const std::string &input, playerflags_t *out)
{
	for (auto &v : STRING_TO_FLAGS_LUT) {
		if (input == v.str) {
			*out = v.flags;
			return true;
		}
	}
	return false;
}
