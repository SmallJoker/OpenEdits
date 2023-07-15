#pragma once

#include <string>

class Packet;
class World;

class EEOconverter {
public:
	EEOconverter(World &world) : m_world(world) {}

	// Read in from EELVL file
	void fromFile(const std::string &filename);

	// Write to new EELVL file
	void toFile(const std::string &filename) const;

	// Utility/debugging function to decompress a file
	static void inflate(const std::string &filename);

private:
	World &m_world;
};
