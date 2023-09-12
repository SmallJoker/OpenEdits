#pragma once

#include <map>
#include <string>

class Packet;
class World;
struct LobbyWorld;

class EEOconverter {
public:
	EEOconverter(World &world) : m_world(world) {}

	// Read in from EELVL file
	void fromFile(const std::string &filename_);

	// Write to new EELVL file
	void toFile(const std::string &filename_) const;

	// Utility/debugging function to decompress a file
	static void inflate(const std::string &filename);

	static void listImportableWorlds(std::map<std::string, LobbyWorld> &worlds);

	static std::string findWorldPath(const std::string &world_id);

	static const std::string IMPORT_DIR;
	static const std::string EXPORT_DIR;

private:
	World &m_world;
};
