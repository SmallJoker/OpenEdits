#include "compressor.h"
#include <memory.h>
#include <stdexcept>
#include <zlib.h>
#include "packet.h"

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) do {} while (false)
#endif
#define ERRORLOG(...) fprintf(stderr, __VA_ARGS__)

constexpr size_t CHUNK_SMALL = 5000;
constexpr size_t CHUNK_BIG = 10 * CHUNK_SMALL;

struct InputOutputData {
	Packet *pkt; // output for compressor, input for decompressor
	bool is_barebone = false;

	bool is_first_chunk = true; // internal, to strip the header
};

// -------------- Compressor (do deflate) -------------

struct InflateWriter {
	InflateWriter()
	{
		memset(&m_zs, 0, sizeof(m_zs));

		// best compression gives header 78 DA when not trimmed
		status = deflateInit(&m_zs, Z_BEST_COMPRESSION);
		if (status != Z_OK)
			throw std::runtime_error("deflateInit failed");
	}

	~InflateWriter()
	{
		// Termination mark
		while (status == Z_OK)
			compress(nullptr, 0);

		deflateEnd(&m_zs);
	}

	// Low-level raw file writing without file header and checksum
	void writeChunk(const uint8_t *data, size_t len)
	{
		if (iodata.is_barebone) {
			if (iodata.is_first_chunk) {
				iodata.is_first_chunk = false;

				// Trim header
				if (len < 2)
					throw std::runtime_error("Failed to strip header");
				data += 2;
				len -= 2;
			}

			if (status == Z_STREAM_END) {
				// Trim checksum
				if (len < 4)
					throw std::runtime_error("Failed to strip checksum");
				len -= 4;
			}
		}

		//DEBUGLOG("zlib: write %zu bytes\n", len);
		iodata.pkt->writeRaw(data, len);
	}

	size_t compress(const uint8_t *src, size_t n_bytes)
	{
		if (src && n_bytes == 0)
			return 0;

		// Stream dead. refuse.
		if (status != Z_OK)
			throw std::runtime_error("Stream ended. Cannot write more.");

		// To terminate the zstream data
		uint8_t tmpbuf[10];
		if (!src) {
			src = tmpbuf;
			n_bytes = 0;
		}

		// https://www.zlib.net/zlib_how.html
		m_zs.next_in = (unsigned char *)src; // zlib might not be compiled with ZLIB_CONST
		m_zs.avail_in = n_bytes;
		size_t n_proc = 0; // Total processed bytes

		do {
			m_zs.avail_out = CHUNK_BIG;
			m_zs.next_out = m_buf_out;

			// Decompress provided data
			status = deflate(&m_zs, src == tmpbuf ? Z_FINISH : Z_NO_FLUSH);
			switch (status) {
				case Z_NEED_DICT:
					status = Z_DATA_ERROR;
					// fall-though
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
				case Z_STREAM_ERROR:
					ERRORLOG("zlib: error code %d, message: %s, near index 0x%04lX\n",
						status, m_zs.msg ? m_zs.msg : "NULL", m_zs.total_in);
					throw std::runtime_error("zlib: inflate error");
			}

			// The amount of newly received bytes
			size_t have = CHUNK_BIG - m_zs.avail_out;
			n_proc = n_bytes - m_zs.avail_in;

			writeChunk(m_buf_out, have);

			if (have > 0)
				DEBUGLOG("zlib: compressed=%zu, remaining=%d, (wrote=%zu)\n", n_proc, m_zs.avail_in, have);
			if (m_zs.avail_in == 0)
				break; // all bytes eaten

		} while (status != Z_STREAM_END);

		return n_proc;
	}


	int status;
	InputOutputData iodata;

private:
	z_stream m_zs;

	// Buffer for compressed data. This is needed to trim header and footer
	// bytes when the barebone mode is specified.
	uint8_t m_buf_out[CHUNK_BIG];
};

Compressor::Compressor(Packet *output, Packet &input) :
	m_input(input)
{
	m_writer = new InflateWriter();
	m_writer->iodata.pkt = output;
}

Compressor::~Compressor()
{
	delete m_writer;
}

void Compressor::setBarebone(bool b)
{
	m_writer->iodata.is_barebone = b;
}

void Compressor::compress()
{
	const uint8_t *buf;
	size_t len;
	do {
		len = m_input.readRawNoCopy(&buf, CHUNK_SMALL);
		m_writer->compress(buf, len);
		DEBUGLOG("Compress n=%zu, remaining=%zu\n", len, m_input.getRemainingBytes());
	} while (len == CHUNK_SMALL);

	delete m_writer;
	m_writer = nullptr;
}

// -------------- Decompressor (do inflate) -------------

struct DeflateReader {
	DeflateReader()
	{
		memset(&m_zs, 0, sizeof(m_zs));

		status = inflateInit(&m_zs);
		if (status != Z_OK)
			throw std::runtime_error("inflateInit failed");
	}

	~DeflateReader()
	{
		inflateEnd(&m_zs);
	}

	// Low-level raw file reading for zlib
	// Adds header and checksum to the deflate data
	size_t readChunk(const uint8_t **data, size_t len)
	{
		if (iodata.is_barebone) {
			if (iodata.is_first_chunk) {
				iodata.is_first_chunk = false;

				// https://yal.cc/cs-deflatestream-zlib/
				tmp_buf[0] = 0x78;
				tmp_buf[1] = 0xDA;

				// https://stackoverflow.com/questions/70347/zlib-compatible-compression-streams
				//data[0] = 0x78;
				//data[1] = 0x01;
				*data = tmp_buf;
				return 2;
			}

			if (iodata.pkt->getRemainingBytes() == 0) {
				// Append missing checksum

				uint32_t to_write = m_zs.adler;
				// LE -> BE
				for (int i = 0; i < 4; ++i)
					tmp_buf[i] = ((const uint8_t *)&to_write)[3 - i];

				*data = tmp_buf;
				return 4;
			}
		}

		// Sets failbit and eofbit on stream end
		return iodata.pkt->readRawNoCopy(data, CHUNK_SMALL);
	}

	/// Returns the N decompressed bytes
	size_t decompress(uint8_t *dst, size_t n_bytes)
	{
		if (n_bytes == 0)
			return 0;

		if (status == Z_STREAM_END)
			return 0;

		// Stream dead. refuse.
		if (status != Z_OK)
			throw std::runtime_error("Stream is dead. Cannot read further.");

		// https://www.zlib.net/zlib_how.html
		size_t n_read = 0; // Total decompressed bytes
		do {
			if (m_zs.avail_in == 0) {
				// All bytes eaten. Read next block
				m_zs.avail_in = readChunk((const uint8_t **)&m_zs.next_in, CHUNK_BIG);

				DEBUGLOG("zlib decompress: in available=%d, out remaining=%zu\n", m_zs.avail_in, n_bytes - n_read);

				if (m_zs.avail_in == 0)
					throw std::runtime_error("zlib data ended unexpectedly");
			}

			m_zs.avail_out = n_bytes - n_read;
			m_zs.next_out = &dst[n_read];

			// Decompress provided data
			status = inflate(&m_zs, Z_NO_FLUSH);
			switch (status) {
				case Z_NEED_DICT:
					status = Z_DATA_ERROR;
					// fall-though
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
				case Z_STREAM_ERROR:
					//ERRORLOG("Got Adler %08lX\n", m_zs.adler);
					ERRORLOG("zlib: error code %d, message: %s, near index 0x%04lX\n",
						status, m_zs.msg ? m_zs.msg : "NULL", m_zs.total_out);
					throw std::runtime_error("zlib: inflate error");
			}

			// The amount of newly received bytes
			size_t have = (n_bytes - n_read) - m_zs.avail_out;
			n_read += have;

			if (n_bytes > 100)
				DEBUGLOG("zlib: decompressed=%zu, remaining=%zu\n", have, n_bytes - n_read);
			if (n_read >= n_bytes)
				break;

		} while (status != Z_STREAM_END);

		// Put back too many read bytes
		if (status == Z_STREAM_END)
			iodata.pkt->readRawNoCopyEnd(m_zs.avail_in);
		return n_read;
	}

	int status;
	InputOutputData iodata;

private:
	uint8_t tmp_buf[4]; // max. 4 for Adler-32
	z_stream m_zs;
};


Decompressor::Decompressor(Packet *output, Packet &input) :
	m_output(*output)
{
	m_reader = new DeflateReader();
	m_reader->iodata.pkt = &input;
}

Decompressor::~Decompressor()
{
	delete m_reader;
}

void Decompressor::setBarebone(bool b)
{
	m_reader->iodata.is_barebone = b;
}

void Decompressor::decompress()
{
	size_t len;
	do {
		len = m_reader->decompress(m_output.writePreallocStart(CHUNK_SMALL), CHUNK_SMALL);
		m_output.writePreallocEnd(len);
		DEBUGLOG("Decompress n=%zu, total=%zu\n", len, m_output.size());
	} while (len > 0);

	delete m_reader;
	m_reader = nullptr;
}
