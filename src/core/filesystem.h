#pragma once

#include <stdint.h>
#include <time.h>
#include <vector>

// Compatibility layer until C++20 is commonly used

struct FileStatInfo {
	time_t ctime, mtime;
};

/// @param info Can be nullptr to check for the file presence
/// @return true on success
bool get_file_stat(const char *filename, FileStatInfo *info);

bool set_file_mtime(const char *filename, time_t time);

bool read_binary_file(const char *filename, std::vector<uint8_t> *dst);
