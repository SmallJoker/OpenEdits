#include "playerflags.h"

std::string PlayerFlags::toHumanReadable() const
{
	std::string out;
	// Check for all temporary flags

	if (check(PF_MUTED))
		out.append("MUTED ");

	if (check(PF_OWNER))
		out.append("owner ");
	else if (check(PF_COOWNER))
		out.append("co-owner ");
	else {
		if (check(PF_EDIT_DRAW))
			out.append("edit-draw ");
		else if (check(PF_EDIT))
			out.append("edit-simple ");
		if (check(PF_GODMODE))
			out.append("godmode ");
	}
	return out;
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
	{ "co-owner",          PlayerFlags::PF_COOWNER },
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
