#include "filesystem.h"

// Thanks to Smeeheey: https://stackoverflow.com/a/40504396
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
	#include <unistd.h>
#endif

#ifdef WIN32
	#define stat _stat
#endif

bool get_file_stat(const char *filename, FileStatInfo *info)
{
	struct stat result;
	if (stat(filename, &result) != 0)
		return false;

	if (info) {
		info->ctime = result.st_ctime;
		info->mtime = result.st_mtime;
	}
	return true;
}

// Thanks to DVK: https://stackoverflow.com/a/2185428
#include <utime.h>

bool set_file_mtime(const char *filename, time_t time)
{
	struct stat old_stat;
	if (stat(filename, &old_stat) != 0)
		return false;

	struct utimbuf file_times;
	file_times.actime = old_stat.st_atime;
	file_times.modtime = time;

	return utime(filename, &file_times) == 0;
}

#include <fstream>
bool read_binary_file(const char *filename, std::vector<uint8_t> *dst)
{
	std::ifstream is(filename, std::ios_base::binary);
	is.seekg(0, std::ios_base::end);
	if (!is.good() && !is.eof())
		return false;

	dst->resize((size_t)is.tellg()); // ::ate

	is.seekg(0, std::ios_base::beg);
	is.read((char *)dst->data(), dst->size());

	size_t bytes_read = is.gcount();
	if (bytes_read != dst->size()) {
		fprintf(stderr, "read_binary_file: size mismatch in '%s'\n", filename);
		return false;
	}

    return true;
}
