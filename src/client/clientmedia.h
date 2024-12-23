#pragma once

#include "core/mediamanager.h"
#include <unordered_set>

class Client;
class Packet;

class ClientMedia : public MediaManager {
public:
	bool download_audiovisuals = true; //< false for headless clients (e.g. bots)

	// List of required media files (header information only)
	void readMediaList(Packet &pkt);

	bool haveMediaForRequest() const { return !m_to_request.empty(); }
	// Name list of the missing files. Packet to server.
	void writeMediaRequest(Packet &pkt);

	// Received data to save to disk
	void readMediaData(Packet &pkt);

	/// Removes old cache files from the disk
	void removeOldCache();

	// Info: `m_media_available` contains the local cache until `readMediaList`.
	size_t countDone() const { return m_got_list ? m_media_available.size() : 0; }
	size_t countMissing() const { return m_to_request.size() + m_pending.size(); }
	size_t bytes_done = 0,
		bytes_missing = 0;

private:
	std::unordered_set<std::string> m_to_request, m_pending;
	bool m_got_list = false;
};
