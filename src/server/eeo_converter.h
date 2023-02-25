#pragma once

#include <string>

class Packet;
class World;

class EEOconverter {
public:
	EEOconverter(World &world) : m_world(world) {}

	void fromFile(const std::string &filename);
	void toFile(const std::string &filename) const;
	static void inflate(const std::string &filename);

private:
	World &m_world;
};
