#include "utils.h"
#include <string.h>
#ifdef _WIN32
	// For string conversion
	#include <windows.h>
#endif

bool strcmpi(const std::string &a, const std::string &b)
{
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); ++i) {
		if (tolower(a[i]) != tolower(b[i]))
			return false;
	}

	return true;
}

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

std::vector<std::string> strsplit(const std::string &input, char delim)
{
	std::vector<std::string> parts;

	const char *pos_a = input.c_str();
	while (*pos_a) {
		const char *pos_b = pos_a;
		while (*pos_b && *pos_b != delim)
			pos_b++;

		// Hit delimiter or end
		std::string val = strtrim(std::string(pos_a, pos_b));
		if (!val.empty()) {
			parts.emplace_back(std::move(val));
		}

		if (!*pos_b)
			break;

		pos_a = pos_b + 1; // skip delimiter
	}
	return parts;
}

void to_player_name(std::string &input)
{
	input = strtrim(input);
	for (char &c : input)
		c = toupper(c);
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

bool string2int64(const char *str, int64_t *val)
{
	if (!*str)
		return false;

	int64_t sval = 0;
	int sign = 1;
	if (*str == '-') {
		sign = -1;
		str++;
	}

	for (; *str; str++) {
		if (!std::isdigit(*str))
			return false;

		sval = sval * 10 + (*str - '0');
	}
	*val = sval * sign;
	return true;
}

static const char *RAND_WORLD_1[] = { "My", "My", "Our", "The Only", "" };
static const char *RAND_WORLD_2[] = { "Amazing", "Awesome", "Cool", "Crazy", "Empty",
	"Fancy", "Free", "Full", "Great", "Large", "Little", "Magnificent", "" };
static const char *RAND_WORLD_3[] = { "World", "World", "Creation", "Art", "Stage" };
std::string generate_world_title()
{
#define PICK_RAND(arr) arr[rand() % (sizeof(arr) / sizeof(arr[0]))]

	std::string result;
	auto pick = PICK_RAND(RAND_WORLD_1);
	if (pick[0])
		result.append(pick);

	pick = PICK_RAND(RAND_WORLD_2);
	if (pick[0]) {
		if (!result.empty())
			result.append(" ");
		result.append(pick);
	}

	pick = PICK_RAND(RAND_WORLD_3);
	if (pick[0]) {
		if (!result.empty())
			result.append(" ");
		result.append(pick);
	}

	return result;

#undef PICK_RAND
}

std::string generate_world_id(unsigned length)
{
	static const char ID_CHARACTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrtstuvwxyz0123456789";
	std::string result(length, '\0');

	for (unsigned i = 0; i < length; ++i)
		result[i] = ID_CHARACTERS[rand() % (sizeof(ID_CHARACTERS) - 1)];

	return result;
}


// ------------------  Time   ------------------


#include <chrono>
#include <thread>
namespace sc = std::chrono;
using timepoint_t = sc::steady_clock::time_point;

void sleep_ms(long delay)
{
	std::this_thread::sleep_for(sc::milliseconds(delay));
}

TimeTaker::TimeTaker(bool do_start)
{
	(timepoint_t *&)m_start_time = new timepoint_t();

	if (do_start)
		start();
}

TimeTaker::~TimeTaker()
{
	delete (timepoint_t *)m_start_time;
}

void TimeTaker::start()
{
	*(timepoint_t *)m_start_time = sc::steady_clock::now();
}

double TimeTaker::stop()
{
	auto stop_time = sc::steady_clock::now();
	return sc::duration_cast<sc::duration<double>>
		(stop_time - *(timepoint_t *)m_start_time).count();
}


// ------------------ Numeric ------------------

// mulberry32 PRNG (CC 0), (C) 2017, Tommy Ettinger
// https://gist.github.com/tommyettinger/46a874533244883189143505d203312c
uint32_t mulberry32_next(uint32_t *state) {
	uint32_t z = (*state += 0x6D2B79F5UL);
	z = (z ^ (z >> 15)) * (z | 1UL);
	z ^= z + (z ^ (z >> 7)) * (z | 61UL);
	return z ^ (z >> 14);
}
