#pragma once

#include <string>

// ------------------ Strings ------------------

std::string strtrim(const std::string &str);

// ------------------ Numeric ------------------

inline float get_sign(float f)
{
	if (f > 0.0001f)
		return 1;
	if (f < -0.0001f)
		return -1;
	return 0;
}
