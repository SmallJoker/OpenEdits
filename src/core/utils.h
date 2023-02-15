#pragma once

#include <string>

// ------------------ Strings ------------------

std::string strtrim(const std::string &str);
std::string get_next_part(std::string &input);

bool utf32_to_utf8(std::string &dst, const wchar_t *str);
bool utf8_to_utf32(std::wstring &dst, const char *str);

bool isalnum_nolocale(const std::string &str);

// ------------------ Numeric ------------------

inline float get_sign(float f)
{
	if (f > 0.0001f)
		return 1;
	if (f < -0.0001f)
		return -1;
	return 0;
}
