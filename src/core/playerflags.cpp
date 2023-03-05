#include "playerflags.h"

std::string PlayerFlags::toHumanReadable() const
{
	std::string temporary;
	// Check for all temporary flags

	if (check(PF_TMP_HEAVYKICK))
		temporary.append("HEAVYKICK ");
	if (check(PF_TMP_MUTED))
		temporary.append("MUTED ");

	if (check(PF_TMP_EDIT_DRAW))
		temporary.append("edit-draw ");
	else if (check(PF_TMP_EDIT))
		temporary.append("edit-simple ");
	if (check(PF_TMP_GODMODE))
		temporary.append("godmode ");

	std::string out;
	if (!temporary.empty())
		out = "( " + temporary + ") ";

	// Persistent flags
	if (check(PF_BANNED))
		out.append("BANNED ");

	if (check(PF_OWNER))
		out.append("owner ");
	else if (check(PF_HELPER))
		out.append("helper ");
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
	{ "banned",          PlayerFlags::PF_BANNED },
	{ "edit-simple",     PlayerFlags::PF_EDIT },
	{ "edit-draw",       PlayerFlags::PF_EDIT_DRAW },
	{ "godmode",         PlayerFlags::PF_GODMODE },
	{ "helper",          PlayerFlags::PF_HELPER },
	{ "owner",           PlayerFlags::PF_OWNER },
	{ "tmp-heavykick",   PlayerFlags::PF_TMP_HEAVYKICK },
	{ "tmp-muted",       PlayerFlags::PF_TMP_MUTED },
	{ "tmp-edit-simple", PlayerFlags::PF_TMP_EDIT },
	{ "tmp-edit-draw",   PlayerFlags::PF_TMP_EDIT_DRAW },
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
