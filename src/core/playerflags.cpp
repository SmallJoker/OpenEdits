#include "playerflags.h"

std::string PlayerFlags::toHumanReadable() const
{
	std::string temporary;
	// Check for all temporary flags

	if (check(PF_TMP_HEAVYKICK))
		temporary.append("HEAVYKICK ");

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
