#include "utils.h"
#include <string.h>

std::string strtrim(const std::string &str)
{
	size_t front = 0;

	while (std::isspace(str[front]))
		++front;

	size_t back = str.size();
	while (back > front && std::isspace(str[back - 1]))
		--back;

	return str.substr(front, back - front);
}

std::string get_next_part(std::string &input)
{
	char *pos_a = &input[0];
	while (*pos_a && std::isspace(*pos_a))
		pos_a++;

	char *pos_b = pos_a;
	while (*pos_b && !std::isspace(*pos_b))
		pos_b++;

	if (!*pos_a) {
		// End reached
		input.clear();
		return "";
	}

	// Split into two parts
	bool ended = !*pos_b;
	*pos_b = '\0';

	std::string value(pos_a); // Until terminator
	if (ended)
		input.clear();
	else
		input = std::string(pos_b + 1);
	return value;
}

// #include <codecvt> is no more in C++17, hence rely on some weird-ass conversion code instead
static_assert(sizeof(wchar_t) == 4);
bool utf32_to_utf8(std::string &dst, const wchar_t *str)
{
	// Source: https://github.com/deqoder/utf8_to_utf32/blob/main/utf8_to_utf32.hpp
	dst.resize(sizeof(wchar_t) * wcslen(str));

	size_t x = 0;
	while (*str) {
		if (*str < 0x80) {
			dst[x++] = *str;
		} else if (*str < 0x800) {
			dst[x++] = (*str >> 6) | 0xc0;
			dst[x++] = (*str & 0x3f) | 0x80;
		} else if (*str < 0x10000) {
			dst[x++] = (*str >> 12) | 0xe0;
			dst[x++] = ((*str >> 6) & 0x3f) | 0x80;
			dst[x++] = (*str & 0x3f) | 0x80;
		} else if (*str < 0x110000) {
			dst[x++] = (*str >> 18) | 0xf0;
			dst[x++] = ((*str >> 12) & 0x3f) | 0x80;
			dst[x++] = ((*str >> 6) & 0x3f) | 0x80;
			dst[x++] = (*str & 0x3f) | 0x80;
		} else {
			dst = "<invalid utf-32 string>";
			return false;
		}
		str++;
	}
	dst.resize(x);
	return true;
}

bool utf8_to_utf32(std::wstring &dst, const char *str)
{
	const size_t length = strlen(str);
	dst.resize(length);

	size_t x = 0;
	for (size_t i = 0; i < length; ) {
		#define CHECKMOVE(len) \
			i += (len); \
			if (i >= length) { \
				dst = L"<invalid utf-8 string>"; \
				return false; \
			}

		uint32_t tmp = (uint32_t)str[i] & 0xff;
		if (tmp < 0x80UL) {
			dst[x++] = str[i];
			i++;
		} else if (tmp < 0xe0UL) {
			CHECKMOVE(2)
			dst[x++] = 0
				| ((str[i-2] & 0x1f) << 6)
				| (str[i-1] & 0x3f)
			;
		} else if (tmp < 0xf0UL) {
			CHECKMOVE(3)
			dst[x++] = 0
				| ((str[i-3] & 0xf) << 12)
				| ((str[i-2] & 0x3f) << 6)
				| (str[i-1] & 0x3f)
			;
		} else if (tmp < 0xf8UL) {
			CHECKMOVE(4)
			dst[x++] = 0
				| ((str[i-4] & 0x7) << 18)
				| ((str[i-3] & 0x3f) << 12)
				| ((str[i-2] & 0x3f) << 6)
				| (str[i-1] & 0x3f)
			;
		} else {
			dst = L"<invalid utf-8 string>"; \
			return false;
		}
		#undef CHECKMOVE
	}
	dst.resize(x);
	return true;
}

static const char *ALNUM_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
bool isalnum_nolocale(const std::string &str)
{
	for (auto c : str) {
		if (!strchr(ALNUM_CHARS, c))
			return false;
	}
	return true;
}
