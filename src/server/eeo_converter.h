#pragma once

#ifdef HAVE_ZLIB

#include <string>

class Packet;
class World;

class EEOconverter {
public:
	static World *import(const std::string &filename);

private:
	static void decompress(Packet &dst, const std::string &filename);
	static World *parse(Packet &pkt);

};

#endif // HAVE_ZLIB
