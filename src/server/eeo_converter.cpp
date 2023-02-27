#include "eeo_converter.h"
#include <stdexcept>

#ifdef HAVE_ZLIB

#include "core/blockmanager.h"
#include "core/world.h"
#include <fstream>
#include <string.h> // memcpy
#include <zlib.h>

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) do {} while (false)
#endif
#define ERRORLOG(...) fprintf(stderr, __VA_ARGS__)

struct CompressedFile {
	CompressedFile(const std::string &filename)
	{
		fp = fopen(filename.c_str(), "r");
	}

	~CompressedFile()
	{
		if (fp)
			fclose(fp);
	}

	size_t readBytes(uint8_t *data, size_t len)
	{
		if (is_first) {
			is_first = false;

			// https://yal.cc/cs-deflatestream-zlib/
			data[0] = 0x78;
			data[1] = 0xDA; // Optimal

			// https://stackoverflow.com/questions/70347/zlib-compatible-compression-streams
			//data[0] = 0x78;
			//data[1] = 0x01;
			return 2;
		}

		if (feof(fp)) {
			if (!zs)
				return 0;

			/* No matter what I tried with "adler32_z", the checksum
			never matched. but apparently reading in works just fine,
			so ignore it and fake it real hard. */

			uint32_t to_write = zs->adler;
			for (int i = 0; i < 4; ++i)
				data[i] = ((const u8 *)&to_write)[3 - i];

			zs = nullptr;
			return 4;
		}

		size_t new_len = fread(data, 1, len, fp);

		return new_len;
	}

	template<typename T>
	void read(T &val);

	FILE *fp;
	z_stream *zs = nullptr;

private:
	bool is_first = true;
};

struct zlibStream {
	zlibStream(const std::string &filename) :
		m_input(filename)
	{
		if (!m_input.fp)
			throw std::runtime_error("Cannot open file: " + filename);

		memset(&m_zs, 0, sizeof(m_zs));
		status = inflateInit(&m_zs);

		if (status != Z_OK)
			throw std::runtime_error("inflateInit failed");

		m_input.zs = &m_zs;
	}

	~zlibStream()
	{
		inflateEnd(&m_zs);
	}

	size_t read(uint8_t *dst, size_t n_bytes)
	{
		if (n_bytes == 0)
			return 0;

		// Stream dead. refuse.
		if (status != Z_OK)
			throw std::runtime_error("Stream ended. Cannot read further.");

		// https://www.zlib.net/zlib_how.html
		size_t n_read = 0;
		do {
			if (m_zs.avail_in == 0) {
				// All bytes eaten. Read next block
				m_zs.avail_in = m_input.readBytes(m_buf_in, CHUNK);
				if (ferror(m_input.fp))
					throw std::runtime_error("File error");

				if (m_zs.avail_in == 0)
					throw std::runtime_error("File ended unexpectedly");

				DEBUGLOG("zlib: reading in %d bytes, space: %zu\n", m_zs.avail_in, n_bytes - n_read);
				m_zs.next_in = m_buf_in;
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
					ERRORLOG("Expected Adler %08lX\n", m_zs.adler);
					ERRORLOG("zlib: error code %d, message: %s, near index 0x%04zX\n",
						status, m_zs.msg ? m_zs.msg : "NULL", getIndex());
					throw std::runtime_error("zlib: inflate error");
			}

			// The amount of newly received bytes
			size_t have = (n_bytes - n_read) - m_zs.avail_out;
			n_read += have;

			if (n_bytes > 100)
				DEBUGLOG("zlib: decompressed %zu bytes (%zu / %zu)\n", have, n_read, n_bytes);
			if (n_read >= n_bytes)
				break;

		} while (status != Z_STREAM_END);

		return n_read;
	}

	inline void be2le(u8 *le_dst, const u8 *be_src, size_t n_bytes)
	{
		for (size_t i = 0; i < n_bytes; ++i)
			le_dst[i] = be_src[(n_bytes - 1) - i];
	}

	// ---------------- Big endian reader functions ----------------
	template<typename T>
	T read()
	{
		u8 bedata[sizeof(T)];

		if (read(bedata, sizeof(T)) != sizeof(T))
			throw std::runtime_error("Failed to read");

		T ret = 0;
		be2le((uint8_t *)&ret, bedata, sizeof(T));
		return ret;
	}

	std::string readStr16()
	{
		u16 len = read<u16>();
		std::string str(len, '\0');
		if (read((uint8_t *)&str[0], len) != len)
			throw std::runtime_error("Failed to read");
		return str;
	}

	std::vector<u16> readArrU16x32()
	{
		u32 len = read<u32>() / sizeof(u16); // bytes -> count

		//DEBUGLOG("Array size: %u at index 0x%04zX\n", len, getIndex());
		std::vector<u16> ret;
		ret.resize(len);
		for (size_t i = 0; i < len; ++i)
			ret[i] = read<u16>();
		return ret;
	}

	int status;
	inline size_t getIndex() { return m_zs.total_out; }

private:

	CompressedFile m_input;
	z_stream m_zs;

	static constexpr size_t CHUNK = 4*1024;
	uint8_t m_buf_in[CHUNK]; // compressed
};

enum class BlockDataType : u8 {
	None,
	I, // Rotation, Number, etc
	III, // Portal
	SI, // Sign, World portal
	SSI, // Label
	SSSS // NPC ??
};

static std::vector<BlockDataType> BLOCK_TYPE_LUT; // Indexed by block ID

static void fill_block_types()
{
	if (!BLOCK_TYPE_LUT.empty())
		return;

	BLOCK_TYPE_LUT.resize(2000);

	auto set_range = [&](BlockDataType type, size_t first, size_t last) {
		while (first <= last) {
			BLOCK_TYPE_LUT.at(first) = type;
			first++;
		}
	};
	auto set_array = [&](BlockDataType type, const u16 which[]) {
		while (*which) {
			BLOCK_TYPE_LUT.at(*which) = type;
			which++;
		}
	};

	// Based on https://github.com/capasha/EEOEditor/blob/e8204a2acb/EELVL/Block.cs#L38-L85
	static const u16 types_I[] = {
		327, 328, 273, 440, 276, 277, 279, 280, 447, 449,
		450, 451, 452, 456, 457, 458, 464, 465, 471, 477,
		475, 476, 481, 482, 483, 497, 492, 493, 494, 1502,
		1500, 1507, 1506, 1581, 1587, 1588, 1592, 1593, 1160,
		1594, 1595, 1597,
		375, 376, 379, 380, 377, 378, 438, 439, 1001, 1002,
		1003, 1004, 1052, 1053, 1054, 1055, 1056, 1092, 275, 329,
		338, 339, 340, 448, 1536, 1537, 1041, 1042, 1043, 1075,
		1076, 1077, 1078, 499, 1116, 1117, 1118, 1119, 1120, 1121,
		1122, 1123, 1124, 1125, 1535, 1135, 1134, 1538, 1140, 1141,
		1155, 1596, 1605, 1606, 1607, 1609, 1610, 1611, 1612, 1614,
		1615, 1616, 1617, 361, 1625, 1627, 1629, 1631, 1633, 1635,
		1101, 1102, 1103, 1104, 1105,
		165, 43, 213, 214, 1011, 1012, 113, 1619, 184, 185,
		467, 1620, 1079, 1080, 1582, 421, 422, 461, 1584,
		423, 1027, 1028, 418, 417, 420, 419, 453, 1517,
		83, 77, 1520,
		0
	};
	set_array(BlockDataType::I, types_I);

	static const u16 types_III[] = { 381, 242, 0 };
	set_array(BlockDataType::III, types_III);

	BLOCK_TYPE_LUT.at(374) = BlockDataType::SI;
	BLOCK_TYPE_LUT.at(385) = BlockDataType::SI;

	BLOCK_TYPE_LUT.at(1000) = BlockDataType::SSI;

	set_range(BlockDataType::SSSS, 1550, 1559);
	set_range(BlockDataType::SSSS, 1569, 1579);
}

void EEOconverter::fromFile(const std::string &filename)
{
	fill_block_types();

	zlibStream zs(filename);

	// https://github.com/capasha/EEOEditor/tree/main/EELVL

	WorldMeta &meta = m_world.getMeta();
	meta.owner = zs.readStr16();
	meta.title = zs.readStr16();
	blockpos_t size;
	size.X = zs.read<s32>();
	size.Y = zs.read<s32>();
	zs.read<float>(); // gravity
	zs.read<u32>(); // bg color
	std::string description = zs.readStr16(); // description
	zs.read<u8>(); // campaign??
	zs.readStr16(); // crew ID
	std::string crew_name = zs.readStr16(); // crew name
	zs.read<s32>(); // crew status
	zs.read<u8>(); // bool: minimap enabled
	zs.readStr16(); // owner ID, often "made offline"

	for (char &c : description) {
		if (c == '\r')
			c = '\\';
	}

	DEBUGLOG("Importing (%d x %d) world by %s\n", size.X, size.Y, meta.owner.c_str());
	DEBUGLOG("\t Title: %s\n", meta.title.c_str());
	DEBUGLOG("\t Description: %s\n", description.c_str());
	DEBUGLOG("\t Crew: %s\n", crew_name.c_str());

	// Actual block data comes now
	m_world.createEmpty(size);

	auto blockmgr = m_world.getBlockMgr();

	std::string err;
	while (zs.status == Z_OK) {
		int block_id;
		try {
			block_id = zs.read<s32>();
		} catch (std::exception &e) {
			err = e.what();
			break;
		}

		int layer = zs.read<s32>();
		DEBUGLOG("\n--> Next block ID: %4d, layer=%d, offset=0x%04zX\n", block_id, layer, zs.getIndex());
		if (layer < 0 || layer > 1)
			throw std::runtime_error("Previous block data mismatch");

		auto pos_x = zs.readArrU16x32();
		auto pos_y = zs.readArrU16x32();
		BlockDataType type = BLOCK_TYPE_LUT[block_id];
		DEBUGLOG("    Data: count=%zu, type=%d, offset=0x%04zX\n",
			pos_x.size(), (int)type, zs.getIndex());

		switch (type) {
			case BlockDataType::None: break;
			case BlockDataType::I:
				zs.read<s32>();
				break;
			case BlockDataType::III:
				zs.read<s32>();
				zs.read<s32>();
				zs.read<s32>();
				break;
			case BlockDataType::SI:
				zs.readStr16();
				zs.read<s32>();
				break;
			case BlockDataType::SSI:
				zs.readStr16();
				zs.readStr16();
				zs.read<s32>();
				break;
			case BlockDataType::SSSS:
				zs.readStr16();
				zs.readStr16();
				zs.readStr16();
				zs.readStr16();
				break;
		}

		auto props = blockmgr->getProps(block_id);
		if (props) {
			if ((layer == 1) != props->isBackground())
				continue; // FG/BG mismatch
		} else {
			// TODO: attempt to remap?
			if (layer == 0) {
				if (block_id < 200)
					block_id = 9;
				else
					block_id = 0; // maybe decoration
			} else {
				block_id = 501;
			}
		}

		for (size_t i = 0; i < pos_x.size(); ++i) {
			Block b;
			blockpos_t pos(pos_x[i], pos_y[i]);
			if (m_world.getBlock(pos, &b)) {
				if (layer == 0)
					b.id = block_id;
				else
					b.bg = block_id;

				m_world.setBlock(pos, b);
			}
		}
	}

	if (zs.status != Z_STREAM_END)
		throw std::runtime_error("zlib: Incomplete reading! msg=" + err);

	return; // Good
}

void EEOconverter::toFile(const std::string &filename) const
{
	throw std::runtime_error("not implemented");
}


void EEOconverter::inflate(const std::string &filename)
{
	fill_block_types();

	zlibStream zs(filename);

	const std::string outname(filename + ".inflated");
	std::ofstream of(outname, std::ios_base::binary);

	if (!of.good())
		throw std::runtime_error("Failed to open destination file");

	while (zs.status != Z_OK || zs.status != Z_STREAM_END) {
		u8 buffer[4*1024];
		size_t read = zs.read(buffer, sizeof(buffer));
		of.write((const char *)buffer, read);
	}

	of.close();

	DEBUGLOG("Exported file %s\n", outname.c_str());
}

#else // HAVE_ZLIB

void EEOconverter::fromFile(const std::string &filename)
{
	throw std::runtime_error("EEOconverter is unavailable");
}

void EEOconverter::toFile(const std::string &filename) const
{
	throw std::runtime_error("EEOconverter is unavailable");
}

void EEOconverter::inflate(const std::string &filename)
{
	throw std::runtime_error("EEOconverter is unavailable");
}

#endif // HAVE_ZLIB
