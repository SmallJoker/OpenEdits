#include "clientmedia.h"
#include "core/filesystem.h"
#include "core/packet.h"
extern "C" {
#include "core/sha3.h"
}
#include <filesystem>
#include <fstream>
#include <string.h> // memcpy
#include <irrTypes.h>
using namespace irr;

extern size_t CONNECTION_MTU;

namespace fs = std::filesystem;
constexpr int SHA3_VARIANT = 256;
const std::string CACHE_DIR = "cache";


namespace {

struct MediaFile {
	std::vector<u8> data;
	u64 data_hash = 0;

	void computeHash()
	{
		data_hash = 0;
		sha3_HashBuffer(SHA3_VARIANT, SHA3_FLAGS_KECCAK,
			data.data(), data.size(),
			&data_hash, sizeof(data_hash)
		);
	}

	// Requires "computeHash" to be run first, or set "data_hash" manually
	std::string getDiskFileName()
	{
		std::string ret;
		ret.append(CACHE_DIR);

		char buf[8 * 2 + 1 + 1];
		snprintf(buf, sizeof(buf), "/%0lx", this->data_hash);
		ret.append(buf);

		return ret;
	}
};

};


void ClientMedia::readMediaList(Packet &pkt)
{
	time_t time_now = time(nullptr);

	while (pkt.getRemainingBytes()) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break;

		size_t size = pkt.read<size_t>();
		MediaFile file;
		pkt.read(file.data_hash);

		std::string cachename = file.getDiskFileName();
		if (fs::exists(cachename) && fs::file_size(cachename) == size) {
			// File in cache matches
			set_file_mtime(cachename.c_str(), time_now);
			m_media.insert({ name, cachename });
			bytes_done += size;
			continue;
		}

		m_to_request.insert(name);
		bytes_missing += size;
	}
}

void ClientMedia::writeMediaRequest(Packet &pkt)
{
	// filename, filename, ....
	auto it = m_to_request.begin();
	for (; it != m_to_request.end(); ++it) {
		// Spread across multiple packets
		if (pkt.size() > CONNECTION_MTU * 5)
			break;

		pkt.writeStr16(*it);
		m_pending.insert(*it);
	}
	// "it" is not removed!
	m_to_request.erase(m_to_request.begin(), it);
}

void ClientMedia::readMediaData(Packet &pkt)
{
	if (!fs::is_directory(CACHE_DIR))
		fs::create_directory(CACHE_DIR);

	while (pkt.getRemainingBytes()) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break; // Terminator

		MediaFile file;
		{
			size_t arrlen = pkt.read<u32>();
			file.data.resize(arrlen);
			pkt.readRaw(file.data.data(), arrlen);

			file.computeHash();
		}

		std::string cachename = file.getDiskFileName();
		if (!file.data.empty()) {
			std::ofstream os(cachename, std::ios_base::binary | std::ios_base::trunc);
			os.write((const char *)file.data.data(), file.data.size());
		}

		m_media.insert({ name, cachename });
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

const char *ClientMedia::getMediaPath(const std::string &filename)
{
	auto it = m_media.find(filename);
	if (it == m_media.end())
		return nullptr;
	return it->first.c_str();
}

