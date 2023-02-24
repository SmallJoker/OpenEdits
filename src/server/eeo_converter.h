#pragma once

#ifdef HAVE_ZLIB

#include <string>

class Packet;
class World;

class EEOconverter {
public:
	EEOconverter(World &world) : m_world(world) {}

	void import(const std::string &filename);
	static void inflate(const std::string &filename);

private:
	World &m_world;
};

#endif // HAVE_ZLIB
