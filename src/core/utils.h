#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ------------------ Strings ------------------

bool strcmpi(const std::string &a, const std::string &b);
std::string::size_type strfindi(const std::string &haystack, const std::string &needle);
std::string strtrim(const std::string &str);
std::string get_next_part(std::string &input);
std::vector<std::string> strsplit(const std::string &input, char delim);

void to_player_name(std::string &input);

bool wide_to_utf8(std::string &dst, const wchar_t *str);
bool utf8_to_wide(std::wstring &dst, const char *str);

bool isalnum_nolocale(const std::string &str);
/// Expects a trimmed string, indicates success
bool string2int64(const char *str, int64_t *val);

std::string generate_world_title();
std::string generate_world_id(unsigned length);

// ------------------  Time   ------------------

void sleep_ms(long delay);

class TimeTaker {
public:
	TimeTaker(bool do_start);
	~TimeTaker();
	void start();
	double stop();

private:
	void *m_start_time = nullptr;
};


// ------------------ Numeric ------------------

inline float get_sign(float f)
{
	if (f > 0.0001f)
		return 1;
	if (f < -0.0001f)
		return -1;
	return 0;
}

// Provided by zlib
extern "C"
unsigned long crc32_z(unsigned long adler, const unsigned char *buf, size_t len);

/// Pseudo-random number generator
// mulberry32 PRNG (CC 0), (C) 2017, Tommy Ettinger
// https://gist.github.com/tommyettinger/46a874533244883189143505d203312c
// Inline this function to increase execution speed by 10x in tight loops
inline uint32_t mulberry32_next(uint32_t *state)
{
	uint32_t z = (*state += 0x6D2B79F5UL);
	z = (z ^ (z >> 15)) * (z | 1UL);
	z ^= z + (z ^ (z >> 7)) * (z | 61UL);
	return z ^ (z >> 14);
}
