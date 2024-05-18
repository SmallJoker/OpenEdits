#include "mediamanager.h"
#include "filesystem.h"
#include "logger.h"
extern "C" {
#include "sha3.h"
}
#include <filesystem>

namespace fs = std::filesystem;
constexpr int SHA3_VARIANT = 256;
const std::string MediaManager::ASSETS_DIR = "assets";

static Logger logger("MediaManager", LL_WARN);

MediaManager::AssetType MediaManager::File::getTypeFromFileName(const std::string &str)
{
	size_t sep = str.find_last_of(".");
	std::string ext = str.substr(sep);

	if (ext == ".png")
		return AssetType::Texture;
	if (ext == ".mp3")
		return AssetType::Sound;
	if (ext == ".lua")
		return AssetType::Script;

	return AssetType::Invalid;
}

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

		auto filename = entry.path().filename();
		AssetType type = File::getTypeFromFileName(filename);
		if (type == AssetType::Invalid)
			continue;

		auto [it, unique] = m_media_available.insert({ filename, entry.path() });
		if (unique) {
			logger(LL_DEBUG, "Found '%s', type=%d",
				filename.c_str(), (int)type
			);
		} else {
			logger(LL_ERROR, "Found duplicate/ambiguous asset: %s", filename.c_str());
		}
	}
	logger(LL_PRINT, "%lu media files indexed", m_media_available.size());
}

const char *MediaManager::getAssetPath(const char *name)
{
	auto it = m_media_available.find(name);
	if (it == m_media_available.end()) {
		logger(LL_WARN, "Cannot find asset '%s'", name);
		return nullptr;
	}

	return it->second.c_str();
}