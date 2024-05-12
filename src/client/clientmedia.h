#pragma once

#include "core/mediamanager.h"
#include <unordered_set>

class Client;
class Packet;

class ClientMedia : public MediaManager {
public:
	// List of required media files (header information only)
	void readMediaList(Packet &pkt);

	bool haveMediaForRequest() const { return !m_to_request.empty(); }
	// Name list of the missing files. Packet to server.
	void writeMediaRequest(Packet &pkt);

	// Received data to save to disk
	void readMediaData(Packet &pkt);

	/// Removes old cache files from the disk
	void removeOldCache();
	const char *getAssetPath(const std::string &filename);

	size_t countDone() const { return m_media_available.size(); }
	size_t countMissing() const { return m_to_request.size() + m_pending.size(); }
	size_t bytes_done = 0,
		bytes_missing = 0;

private:
	std::unordered_set<std::string> m_to_request, m_pending;
};
