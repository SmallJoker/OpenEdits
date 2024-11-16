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

	/// Maximum bytes to process (default: SIZE_MAX)
	/// Avoids extracting zip bombs.
	void setLimit(size_t n_bytes) { m_limit_bytes = n_bytes; }

	/// b = true: When the data does not include the zlib header
	///           or the Adler32 checksum at the end.
	void setBarebone(bool b = true);

	void decompress();

private:
	DeflateReader *m_reader = nullptr;
	Packet &m_output;
	size_t m_limit_bytes = SIZE_MAX;
};
