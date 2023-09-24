#pragma once
#include <cstdint>
#include <cstddef>

struct DeflateReader;
struct InflateWriter;
class Packet;

class Compressor {
public:
	Compressor(Packet *output, Packet &input);
	~Compressor();

	void setBarebone(bool b = true);
	void compress();

private:
	InflateWriter *m_writer = nullptr;
	Packet &m_input;
};

class Decompressor {
public:
	Decompressor(Packet *output, Packet &input);
	~Decompressor();

	void setBarebone(bool b = true);
	void decompress();

private:
	DeflateReader *m_reader = nullptr;
	Packet &m_output;
};
