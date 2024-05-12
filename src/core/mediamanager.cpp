#include "mediamanager.h"
#include "filesystem.h"
extern "C" {
#include "sha3.h"
}
#include <filesystem>

namespace fs = std::filesystem;
constexpr int SHA3_VARIANT = 256;
const std::string MediaManager::ASSETS_DIR = "assets";

void MediaManager::File::computeHash()
{
	data_hash = 0;
	file_size = data.size();
	sha3_HashBuffer(SHA3_VARIANT, SHA3_FLAGS_KECCAK,
		data.data(), data.size(),
		&data_hash, sizeof(data_hash)
	);
}

void MediaManager::File::uncacheRAMif(time_t olderthan)
{
	if (cache_last_hit > olderthan)
		return; // accessed more recently

	// free data
	std::vector<uint8_t>().swap(data);
}

bool MediaManager::File::cacheToRAM()
{
	if (file_size == 0)
		return false; // not found
	if (!data.empty())
		return true; // already one

	// do cache again (after uncacheRAMif())
	bool ok = read_binary_file(file_path.c_str(), &data);
	if (!ok) {
		// file got removed
		file_size = 0;
		uncacheRAMif(0); // always
	} else if (file_size != data.size()) {
		// file contents were changed
		computeHash();
	}
	return ok;
}

void MediaManager::indexAssets()
{
	for (const auto &entry : fs::recursive_directory_iterator(ASSETS_DIR)) {
		if (!entry.is_regular_file())
			continue;

		auto ext = entry.path().extension();
		if (ext != ".png" && ext != ".lua" && ext != ".mp3")
			continue;

		/*printf("indexAssets: '%s'\n",
			entry.path().c_str()
		);*/

		m_media_available.insert({ entry.path().filename(), entry.path() });
	}
	printf("MediaManager: %lu media files indexed\n", m_media_available.size());
}
