#include "servermedia.h"
#include "remoteplayer.h"
#include "core/packet.h"
extern "C" {
#include "core/sha3.h"
}
#include <filesystem>

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
		if (ext != "png" && ext != "lua" && ext != "mp3")
			continue;

		printf("indexAssets: '%s'\n",
			entry.path().c_str()
		);

		m_media_available.insert({ entry.path().filename(), entry.path() });
	}
}

void ServerMedia::requireMedia(const char *name)
{
	if (name[0] == '\0')
		return; // Not allowed

	std::string name_s = name;
	auto it = m_media_available.find(name_s);
	if (it == m_media_available.end())
		return;

	ServerMediaFile file;
	// TODO: open file and populate hash
	m_required.insert({ name_s, std::move(file) });
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

		player->pending_media.insert(filename);
	}
}

void ServerMedia::writeMediaData(RemotePlayer *player, Packet &pkt)
{
	// name, size, binary

	auto &list = player->pending_media;
	auto it = list.begin();
	for (; it != list.end(); ++it) {
		// Spread across multiple packets
		if (pkt.size() > CONNECTION_MTU * 10)
			break;

		pkt.writeStr16(*it); // name
		size_t arrlen = 0;
		const u8 *data = nullptr;

		auto file_it = m_required.find(*it);
		if (file_it != m_required.end()) {
			arrlen = file_it->second.data.size();
			data = file_it->second.data.data();
		}

		pkt.write<u32>(arrlen); // size
		pkt.writeRaw(data, arrlen); // binary
	}

	list.erase(list.begin(), it);
}
