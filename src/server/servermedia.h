#pragma once

#include "core/mediamanager.h"

class Packet;
class RemotePlayer;

class ServerMedia : public MediaManager {
public:
	/// To call from the Lua registration functions
	/// Compose a list of required media files for the clients
	/// @return true if found
	bool requireAsset(const char *name) override;

	void writeMediaList(RemotePlayer *player, Packet &pkt);
	void readMediaRequest(RemotePlayer *player, Packet &pkt);
	void writeMediaData(RemotePlayer *player, Packet &pkt);

	/// Removes rarely used media files from RAM
	void uncacheMedia();

private:
	/// Subset of `m_media_available` that is actually "in use".
	std::map<std::string, File> m_required;
};
