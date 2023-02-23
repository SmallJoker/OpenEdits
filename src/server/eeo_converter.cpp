#ifdef HAVE_ZLIB

#include "eeo_converter.h"
#include "core/packet.h"
#include "core/world.h"
#include <string>
#include <string.h> // memcpy
#include <zlib.h>

struct CompressedFile {
	CompressedFile(const std::string &filename)
	{
		fp = fopen(filename.c_str(), "r");
	}

	~CompressedFile()
	{
		fclose(fp);
	}

	size_t readBytes(uint8_t *data, size_t len)
	{
		// https://stackoverflow.com/questions/70347/zlib-compatible-compression-streams
		if (is_first) {
			is_first = false;
			*data++ = 0x78;
			*data++ = 0x01;
			return 2;
		}
		if (is_last) {
			is_last = false;
			// last few bytes
			uint32_t adler32 = (b << 16) | a;
			memcpy(data, &adler32, sizeof(adler32));
			return 4;
		}

		size_t new_len = fread(data, 1, len, fp);

		for (size_t i = 0; i < new_len; ++i) {
			a = (a + (data[i])) % MODULUS;
			b = (b + a) % MODULUS;
		}

		if (new_len < len)
			is_last = true;

		return new_len;
	}

	FILE *fp;

private:
	bool is_first = true;
	bool is_last = false;

	uint32_t a = 1, b = 0;
	static constexpr uint32_t MODULUS = 65521;

};

struct zlibCleanupper {
	zlibCleanupper(z_stream *zs) : zs(zs) {}

	~zlibCleanupper()
	{
		inflateEnd(zs);
	}

	z_stream *zs;
};


World *EEOconverter::import(const std::string &filename)
{
	Packet pkt;
	decompress(pkt, filename);

	return parse(pkt);
}

void EEOconverter::decompress(Packet &dst, const std::string &filename)
{

	// https://www.zlib.net/zlib_how.html
	z_stream zs;
	memset(&zs, 0, sizeof(zs));

	const size_t CHUNK = 16384;
	uint8_t buf_in[CHUNK];
	uint8_t buf_out[CHUNK];

	int ret = inflateInit(&zs);

	if (ret != Z_OK)
		throw std::runtime_error("zlib: inflateInit failed");

	zlibCleanupper zlibcleanup(&zs);

	CompressedFile source(filename);
	if (!source.fp)
		throw std::runtime_error("zlib: input file not found");

	std::string filename_dest(filename + ".inflated");
	FILE *dest = fopen(filename_dest.c_str(), "w");
	if (!dest)
		throw std::runtime_error("zlib: cannot create destination file");


	do {
		zs.avail_in = source.readBytes(buf_in, CHUNK);
		if (ferror(source.fp))
			goto end;

		if (zs.avail_in == 0)
			break;

		printf("proc %d bytes\n", zs.avail_in);
		zs.next_in = buf_in;

		zs.avail_out = CHUNK;
		zs.next_out = buf_out;

		// Decompress provided data
		ret = inflate(&zs, Z_NO_FLUSH);
		switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;
				// fall-though
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
			case Z_STREAM_ERROR:
				goto end;
		}

		// Write back the decompressed stream
		size_t have = CHUNK - zs.avail_out;
		printf("zlib: in %d bytes ---> out %zu bytes\n", zs.avail_in, have);
		if (fwrite(buf_out, 1, have, dest) != have || ferror(dest)) {
			puts("fwrite failed");
			goto end;
		}

	} while (ret != Z_STREAM_END);

end:
	fclose(dest);

	if (ret != Z_STREAM_END) {
		printf("zlib inflate finished with code %d\n", ret);
		throw std::runtime_error("zlib: reading terminated incorrectly");
	}
}

World *EEOconverter::parse(Packet &pkt)
{
	// https://github.com/capasha/EEOEditor/tree/main/EELVL
	return nullptr;
}


#endif // HAVE_ZLIB
