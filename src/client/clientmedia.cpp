#include "clientmedia.h"
#include "core/filesystem.h"
#include "core/packet.h"
#include <filesystem>
#include <fstream>
#include <string.h> // memcpy

extern size_t CONNECTION_MTU;

namespace fs = std::filesystem;
const std::string CACHE_DIR = "cache";


// Requires "computeHash" to be run first, or set "data_hash" manually
std::string MediaManager::File::getDiskFileName()
{
	std::string ret;
	ret.append(CACHE_DIR);

	char buf[8 * 2 + 1 + 1];
	snprintf(buf, sizeof(buf), "/%0lx", this->data_hash);
	ret.append(buf);

	return ret;
}

// -------------- ClientMedia --------------

void ClientMedia::readMediaList(Packet &pkt)
{
	time_t time_now = time(nullptr);

	// name, size, data hash
	while (pkt.getRemainingBytes()) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break;

		size_t size = pkt.read<uint32_t>();
		File file;
		pkt.read<uint64_t>(file.data_hash);

		std::string cachename = file.getDiskFileName();
		if (fs::exists(cachename) && fs::file_size(cachename) == size) {
			// File in cache matches
			set_file_mtime(cachename.c_str(), time_now);
			// overwrite path of the indexed local files
			m_media_available[name] = cachename;
			bytes_done += size;
			continue;
		}

		// TODO: also check the local files to avoid caching them again

		m_to_request.insert(name);
		bytes_missing += size;
	}
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
			printf("ClientMedia: cache file '%s', size=%ld\n", name.c_str(), file.data.size());
		}

		m_media_available.insert({ name, cachename });
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
		auto path_c = entry.path().c_str();
		if (!get_file_stat(path_c, &info))
			continue;

		time_t age_days = (time_now - info.mtime) / (3600 * 24);
		if (age_days < 60)
			continue;

		int status = std::remove(path_c);
		printf("removeOldCache: '%s', age=%ldd, status=%d\n",
			entry.path().filename().c_str(), age_days, status
		);
	}
}
