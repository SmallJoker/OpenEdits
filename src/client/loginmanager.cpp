#include "loginmanager.h"
#include "client/client.h" // ClientStartData
#include "core/systemglue.h" // get_user_paths
#include "core/packet.h"
#include "core/std_helpers.h" // packet_from_istream
#include "core/utils.h" // strsplit
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <stdio.h> // sscanf

static const uint8_t LM_VERSION_NEWEST = 1;
#define FILENAME_FMT "client_logins.v%u.pktdb"

namespace fs = std::filesystem;

LoginManager::LoginManager()
{
}

LoginManager::~LoginManager()
{
}

void LoginManager::clear()
{
	m_filename.clear();
	m_entries.clear();
	m_loaded = false;
}

void LoginManager::load()
{
	const auto &user_paths = get_user_paths(true);

	int64_t found_version = -1;
	fs::path found_path;

	for (const std::string &path : user_paths) {
		for (const auto &entry : fs::directory_iterator(path)) {
			if (!entry.is_regular_file())
				continue;

			auto filename = entry.path().filename().string();

			unsigned ver;
			unsigned chars_read = 0;
			sscanf(filename.c_str(), FILENAME_FMT "%n", &ver, &chars_read);
			if (chars_read == 0)
				continue;

			if (ver > LM_VERSION_NEWEST || ver < found_version)
				continue;

			found_version = ver;
			found_path = entry.path();
		}
	}


	if (found_version > 0) {
		m_filename = found_path.string();
		loadSerialized();
	} else {
		m_filename = "client_servers.txt";
		loadFromTextfile();
	}
	printf("Found file '%s', version=%d\n", m_filename.c_str(), (int)found_version);
	m_loaded = true;

	if (found_version < LM_VERSION_NEWEST) {
		// Migrate: save data under a new file name
		const std::string old_filename = m_filename;

		char buf[128];
		snprintf(buf, sizeof(buf), FILENAME_FMT, LM_VERSION_NEWEST);

		m_filename = user_paths[0] + "/" + buf;
		save();

		std::remove(old_filename.c_str());
	}
}

void LoginManager::save() const
{
	ASSERT_FORCED(!m_filename.empty(), "missing path");

	saveSerialized();
}

ClientStartData *LoginManager::find(const std::string &address, const std::string &name)
{
	for (auto &entry : m_entries) {
		if (entry.address == address && entry.nickname == name)
			return &entry;
	}
	return nullptr;
}

void LoginManager::recordLogin(ClientStartData &&data)
{
	ClientStartData *entry = find(data.address, data.nickname);
	if (entry)
		*entry = std::move(data);
	else
		m_entries.emplace_back(std::move(data));
}

void LoginManager::removeEntry(size_t i)
{
	ASSERT_FORCED(i < m_entries.size(), "out of bounds");
	m_entries.erase(m_entries.begin() + i);
}

static ClientStartData get_from_line(std::string &line)
{
	ClientStartData start_data;
	start_data.address = get_next_part(line).c_str();
	start_data.nickname = get_next_part(line).c_str();
	return start_data;
}

bool LoginManager::loadFromTextfile()
{
	std::ifstream is(m_filename);
	if (!is.good())
		return false;

	m_entries.clear();

	std::string line;
	while (std::getline(is, line)) {
		ClientStartData info = get_from_line(line);
		if (info.nickname.empty())
			continue;

		m_entries.push_back(std::move(info));
	}

	return true;
}

bool LoginManager::loadSerialized()
{
	std::ifstream is(m_filename, std::ios_base::binary);
	if (!is.good())
		return false;

	m_entries.clear();

	Packet pkt = packet_from_istream(is);
	is.close();

	const u8 version = pkt.read<u8>();
	ASSERT_FORCED(version <= LM_VERSION_NEWEST, "invalid version");

	const u16 len = pkt.read<u16>();
	m_entries.reserve(len);
	for (unsigned i = 0; i < len; ++i) {
		auto &info = m_entries.emplace_back();
		info.address = pkt.readStr16();
		pkt.readStr16();
		info.nickname = pkt.readStr16();
		info.token = pkt.readStr16();
		const u8 flags = pkt.read<u8>();
		info.remember_password = flags & 0x01;
	}
	return true;
}

void LoginManager::saveSerialized() const
{
	Packet pkt(4000);
	pkt.write(LM_VERSION_NEWEST);
	pkt.write<u16>(m_entries.size());

	for (auto &info : m_entries) {
		pkt.writeStr16(info.address);
		pkt.writeStr16(""); // db key
		pkt.writeStr16(info.nickname);
		pkt.writeStr16(info.token);
		pkt.write<u8>(
			0
			| (0x01 * info.remember_password)
		);
	}

	std::ofstream os(m_filename, std::ios_base::binary);
	const uint8_t *data = nullptr;
	pkt.readRawNoCopy(&data, pkt.size());
	os.write((const char *)data, pkt.size());
}

