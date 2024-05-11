#pragma once

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

class Client;
class Packet;

class ClientMedia {
public:
	// List of required media files (header information only)
	void readMediaList(Packet &pkt);

	bool haveMediaForRequest() const { return !m_to_request.empty(); }
	// Name list of the missing files. Packet to server.
	void writeMediaRequest(Packet &pkt);

	// Received data to save to disk
	void readMediaData(Packet &pkt);

	void removeOldCache();
	const char *getMediaPath(const std::string &filename);

	size_t countDone() const { return m_media.size(); }
	size_t countMissing() const { return m_to_request.size() + m_pending.size(); }
	size_t bytes_done = 0,
		bytes_missing = 0;

private:
	std::unordered_set<std::string> m_to_request, m_pending;

	// key: filename, value: full path to the file on disk
	std::map<std::string, std::string> m_media;
};
