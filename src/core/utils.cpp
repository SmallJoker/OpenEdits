#include "utils.h"
#include <string.h>
#ifdef _WIN32
	// For string conversion
	#include <windows.h>
#endif

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

#ifndef _WIN32

// #include <codecvt> is no more in C++17, hence rely on some weird-ass conversion code instead
static_assert(sizeof(wchar_t) == 4);
bool wide_to_utf8(std::string &dst, const wchar_t *str)
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

bool utf8_to_wide(std::wstring &dst, const char *str)
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

#else

// Based on code written by est31, https://github.com/minetest/minetest/commit/572990dcd
bool utf8_to_wide(std::wstring &dst, const char *str)
{
	const size_t length = strlen(str);
	size_t outbuf_size = length + 1;
	wchar_t *outbuf = new wchar_t[outbuf_size];
	memset(outbuf, 0, outbuf_size * sizeof(wchar_t));
	// Windows does not seem to like "CP_UTF8" -> results in unittest fail
	int hr = MultiByteToWideChar(CP_ACP, 0, str, length,
		outbuf, outbuf_size);
	if (hr >= 0)
		dst.assign(outbuf);
	delete[] outbuf;
	return hr >= 0;
}

bool wide_to_utf8(std::string &dst, const wchar_t *str)
{
	const size_t length = wcslen(str);
	size_t outbuf_size = (length + 1) * 6;
	char *outbuf = new char[outbuf_size];
	memset(outbuf, 0, outbuf_size);
	// Windows does not seem to like "CP_UTF8" -> results in unittest fail
	int hr = WideCharToMultiByte(CP_ACP, 0, str, length,
		outbuf, outbuf_size, NULL, NULL);
	if (hr >= 0)
		dst.assign(outbuf);
	delete[] outbuf;
	return hr >= 0;
}


#endif

bool isalnum_nolocale(const std::string &str)
{
	for (auto c : str) {
		if (c >= 'A' && c <= 'Z')
			continue;
		if (c >= 'a' && c <= 'z')
			continue;
		if (c >= '0' && c <= '9')
			continue;

		return false;
	}
	return true;
}
