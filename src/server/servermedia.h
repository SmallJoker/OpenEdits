#pragma once

#include <irrTypes.h>
#include <map>
#include <string>
#include <vector>
using namespace irr;

class Packet;
class RemotePlayer;

struct ServerMediaFile {
	size_t file_size = 0;
	u64 data_hash = 0;
	std::string file_path;

	// For caching purposes
	std::vector<u8> data;
	time_t cache_last_hit = 0;

	void computeHash();
};

class ServerMedia {
public:
	/// Find all available assets for distribution
	void indexAssets();

	/// To call from the Lua registration functions
	/// Compose a list of required media files for the clients
	/// @return true if found
	bool requireMedia(const char *name);

	void writeMediaList(Packet &pkt);
	void readMediaRequest(RemotePlayer *player, Packet &pkt);
	void writeMediaData(RemotePlayer *player, Packet &pkt);

	/// Removes rarely used media files from RAM
	void uncacheMedia();

private:
	// key: filename, value: full path to the file on disk
	std::map<std::string, std::string> m_media_available;
	std::map<std::string, ServerMediaFile> m_required;
};
