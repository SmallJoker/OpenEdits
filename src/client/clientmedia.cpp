#include "clientmedia.h"
#include "core/filesystem.h"
#include "core/logger.h"
#include "core/packet.h"
#include <filesystem>
#include <fstream>
#include <string.h> // memcpy

extern size_t CONNECTION_MTU;

namespace fs = std::filesystem;
const std::string CACHE_DIR = "cache";
static Logger logger("ClientMedia", LL_INFO);

// Requires "computeHash" to be run first, or set "data_hash" manually
std::string MediaManager::File::getDiskFileName()
{
	std::string ret;
	ret.append(CACHE_DIR);

	char buf[8 * 2 + 1 + 1];
	snprintf(buf, sizeof(buf), "/%0x", this->data_hash);
	ret.append(buf);

	return ret;
}

// -------------- ClientMedia --------------

void ClientMedia::readMediaList(Packet &pkt)
{
	time_t time_now = time(nullptr);

	decltype(m_media_available) local_media;
	std::swap(m_media_available, local_media);
	// m_media_available is now EMPTY

	// name, size, data hash
	while (pkt.getRemainingBytes()) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break;

		size_t size = pkt.read<uint32_t>();
		uint32_t data_hash = pkt.read<uint32_t>();

		File file;
		file.file_size = size;
		file.data_hash = data_hash;

		// Check file in cache
		file.file_path = file.getDiskFileName();
		if (file.cacheToRAM() && file.data_hash == data_hash && file.file_size == size) {
			// Delay file deletion
			set_file_mtime(file.file_path.c_str(), time_now);
			// overwrite path of the indexed local files
			m_media_available[name] = file.file_path;
			bytes_done += size;
			logger(LL_DEBUG, "%s: file %s from cache\n", __func__, name.c_str());
			continue;
		}

		// Check the local files to avoid unnecessary caching
		auto it = local_media.find(name);
		if (it != local_media.end()) {
			file.file_size = size;
			file.file_path = it->second;
			// Cache function oveerwrites the hash!
			if (file.cacheToRAM() && file.data_hash == data_hash && file.file_size == size) {
				m_media_available[name] = it->second;
				bytes_done += size;
				logger(LL_DEBUG, "%s: file %s from distr\n", __func__, name.c_str());
				continue;
			}
		}

		m_to_request.insert(name);
		bytes_missing += size;
	}

	m_got_list = true;
}

void ClientMedia::writeMediaRequest(Packet &pkt)
{
	// filename, filename, ....
	for (const std::string &fn : m_to_request) {
		pkt.writeStr16(fn);
		m_pending.insert(fn);
	}
	m_to_request.clear();
}

void ClientMedia::readMediaData(Packet &pkt)
{
	if (!fs::is_directory(CACHE_DIR))
		fs::create_directory(CACHE_DIR);

	// name, size, [bytes ...]

	while (pkt.getRemainingBytes()) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break; // Terminator

		File file;
		{
			size_t arrlen = pkt.read<uint32_t>();
			file.data.resize(arrlen);
			pkt.readRaw(file.data.data(), arrlen);

			file.computeHash();
		}

		std::string cachename = file.getDiskFileName();
		if (!file.data.empty()) {
			std::ofstream os(cachename, std::ios_base::binary | std::ios_base::trunc);
			os.write((const char *)file.data.data(), file.data.size());
			logger(LL_DEBUG, "cache file '%s', size=%zu", name.c_str(), file.data.size());
		}

		m_media_available[name] = cachename; // overwrite in case when using local files
		bytes_done    += file.data.size();
		bytes_missing -= file.data.size();
		m_pending.erase(name);
		m_to_request.erase(name);
	}
}

void ClientMedia::removeOldCache()
{
	if (!fs::is_directory(CACHE_DIR))
		return;

	const time_t time_now = time(nullptr);

	for (const auto &entry : fs::directory_iterator(CACHE_DIR)) {
		if (!entry.is_regular_file())
			continue;

		FileStatInfo info;
		auto path_c = entry.path().string();
		if (!get_file_stat(path_c.c_str(), &info))
			continue;

		long age_days = (time_now - info.mtime) / (3600 * 24);
		if (age_days < 60)
			continue;

		int status = std::remove(path_c.c_str());
		logger(LL_DEBUG, "remove cached file: '%s', age=%ldd, status=%d",
			entry.path().filename().string().c_str(), age_days, status
		);
	}
}
