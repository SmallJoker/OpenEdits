#include "std_helpers.h"
#include <iostream>

Packet packet_from_istream(std::istream &is)
{
	is.seekg(0, is.end);
	const size_t length = is.tellg();
	is.seekg(0, is.beg);
	Packet pkt(length);

	do {
		is.read((char *)pkt.writePreallocStart(length), length);
		pkt.writePreallocEnd(is.gcount()); // already written
	} while (is.good());

	if (pkt.size() != length)
		fprintf(stderr, "File changed size?! before=%zu after=%zu\n", length, is.gcount());
	return pkt;
}

