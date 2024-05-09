#pragma once

#include <time.h>

// Compatibility layer until C++20 is commonly used

struct FileStatInfo {
	time_t ctime, mtime;
};

bool get_file_stat(const char *filename, FileStatInfo *info);

bool set_file_mtime(const char *filename, time_t time);
