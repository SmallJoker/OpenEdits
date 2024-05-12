#include "servermedia.h"
#include "remoteplayer.h"
#include "core/filesystem.h"
#include "core/packet.h"
#include <filesystem>

extern size_t CONNECTION_MTU;

namespace fs = std::filesystem;

// -------------- ServerMedia --------------

bool ServerMedia::requireAsset(const char *name)
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

	File file;
	file.file_path = it->second;
	file.file_size = SIZE_MAX; // non-zero
	bool ok = file.cacheToRAM();

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

		File &file = file_it->second;

		file.cacheToRAM();
		file.cache_last_hit = time_now;

		pkt.write<u32>(file.data.size()); // size
		pkt.writeRaw(file.data.data(), file.data.size()); // binary

		player->total_sent_media++;
	}

	list.erase(list.begin(), it);
}

void ServerMedia::uncacheMedia()
{
	const time_t time_old = time(nullptr) - (30 * 60);

	for (auto &kv : m_required) {
		kv.second.uncacheRAMif(time_old);
	}
}
