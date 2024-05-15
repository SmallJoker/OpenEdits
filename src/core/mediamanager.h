#pragma once

#include <map>
#include <string>
#include <vector>
#include <stddef.h> // size_t
#include <stdint.h> // *int*_t
#include <time.h>   // time_t

class MediaManager {
public:
	struct File {
		size_t file_size = 0;
		uint64_t data_hash = 0;
		std::string file_path;

		// For caching purposes
		std::vector<uint8_t> data;
		time_t cache_last_hit = 0;

		/// Trimmed SHA256 hash based on the contents in "data"
		void computeHash();

		/// Clears the data when
		void uncacheRAMif(time_t olderthan);

		/// @returns true if cached
		bool cacheToRAM();

#if BUILD_CLIENT
		// Client-only
		std::string getDiskFileName();
#endif
	};

	virtual ~MediaManager() = default;

	/// Indexes `ASSETS_DIR` to list all media in `m_media_available`
	void indexAssets();

	virtual bool requireAsset(const char *name) { return false; }

	/// @return true on success. "out" contains the file path (if non-NULL)
	virtual bool getAssetPath(const char *name, std::string *out);

	static const std::string ASSETS_DIR;

protected:
	MediaManager() = default;

	// key: filename, value: full path to the file on disk
	std::map<std::string, std::string> m_media_available;
};
