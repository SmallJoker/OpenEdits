#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ------------------ Strings ------------------

bool strcmpi(const std::string &a, const std::string &b);
std::string strtrim(const std::string &str);
std::string get_next_part(std::string &input);
std::vector<std::string> strsplit(const std::string &input, char delim);

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
