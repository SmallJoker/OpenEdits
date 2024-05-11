#include "servermedia.h"
#include "remoteplayer.h"
#include "core/filesystem.h"
#include "core/packet.h"
extern "C" {
#include "core/sha3.h"
}
#include <filesystem>
#include <fstream>

extern size_t CONNECTION_MTU;

namespace fs = std::filesystem;
constexpr int SHA3_VARIANT = 256;
const std::string ASSETS_DIR = "assets";


void ServerMediaFile::computeHash()
{
	data_hash = 0;
	file_size = data.size();
	sha3_HashBuffer(SHA3_VARIANT, SHA3_FLAGS_KECCAK,
		data.data(), data.size(),
		&data_hash, sizeof(data_hash)
	);
}


void ServerMedia::indexAssets()
{
	for (const auto &entry : fs::recursive_directory_iterator(ASSETS_DIR)) {
		if (!entry.is_regular_file())
			continue;

		auto ext = entry.path().extension();
		if (ext != ".png" && ext != ".lua" && ext != ".mp3")
			continue;

		printf("indexAssets: '%s'\n",
			entry.path().c_str()
		);

		m_media_available.insert({ entry.path().filename(), entry.path() });
	}
}

bool ServerMedia::requireMedia(const char *name)
{
	if (name[0] == '\0')
		return false; // Not allowed

	std::string name_s = name;

	auto rit = m_required.find(name_s);
	if (rit != m_required.end())
		return true; // already recorded

	auto it = m_media_available.find(name_s);
	if (it == m_media_available.end())
		return false;

	ServerMediaFile file;
	file.file_path = it->second;
	bool ok = read_binary_file(file.file_path.c_str(), &file.data);
	if (ok)
		file.computeHash();

	printf("ServerMedia: require '%s', size=%ld\n", name, file.data.size());
	m_required.insert({ name_s, std::move(file) });
	return ok;
}

void ServerMedia::writeMediaList(Packet &pkt)
{
	// name, size, data hash
	for (const auto &[name, file] : m_required) {
		pkt.writeStr16(name);
		pkt.write<u32>(file.file_size);
		pkt.write<u64>(file.data_hash);
	}
}

void ServerMedia::readMediaRequest(RemotePlayer *player, Packet &pkt)
{
	while (pkt.getRemainingBytes()) {
		std::string filename = pkt.readStr16();
		if (filename.empty())
			break;

		player->requested_media.insert(filename);
	}
}

void ServerMedia::writeMediaData(RemotePlayer *player, Packet &pkt)
{
	// name, size, [bytes ...]
	if (player->total_sent_media >= m_required.size()) {
		// Avoid request bombs. In theory we sent all media, thus stop.
		pkt.writeStr16(""); // terminate
		return;
	}

	const time_t time_now = time(nullptr);

	auto &list = player->requested_media;
	auto it = list.begin();
	for (; it != list.end(); ++it) {
		// Spread across multiple packets
		if (pkt.size() > CONNECTION_MTU * 10)
			break;

		pkt.writeStr16(*it); // name

		auto file_it = m_required.find(*it);
		if (file_it == m_required.end()) {
			pkt.write<u32>(0);
			// no data
			continue;
		}

		ServerMediaFile &file = file_it->second;

		if (file.file_size > 0 && file.data.empty()) {
			// do cache again (after uncacheMedia())
			bool ok = read_binary_file(file.file_path.c_str(), &file.data);
			if (!ok) {
				// file got removed
				file.file_size = 0;
			} else if (file.file_size != file.data.size()) {
				// file contents were changed
				file.computeHash();
			}
		}

		file.cache_last_hit = time_now;

		pkt.write<u32>(file.data.size()); // size
		pkt.writeRaw(file.data.data(), file.data.size()); // binary

		player->total_sent_media++;
	}

	list.erase(list.begin(), it);
}

void ServerMedia::uncacheMedia()
{
	const time_t time_now = time(nullptr);

	for (auto &kv : m_required) {
		if (time_now - kv.second.cache_last_hit > 30 * 60) {
			// expired. free data.
			std::vector<uint8_t>().swap(kv.second.data);
		}
	}
}
