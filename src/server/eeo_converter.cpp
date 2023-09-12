#include "eeo_converter.h"
#include <stdexcept>

#ifdef HAVE_ZLIB

#include "core/blockmanager.h"
#include "core/world.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <string.h> // memcpy
#include <zlib.h>

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) do {} while (false)
#endif
#define ERRORLOG(...) fprintf(stderr, __VA_ARGS__)


const std::string EEOconverter::IMPORT_DIR = "worlds/imports";
const std::string EEOconverter::EXPORT_DIR = "worlds/exports";

struct InflateWriter {
	InflateWriter(const std::string &outputfile) :
		m_file(outputfile, std::ios::binary)
	{
		memset(&m_zs, 0, sizeof(m_zs));

		if (!m_file.good())
			throw std::runtime_error("Cannot open file: " + outputfile);

		// best compression gives header 78 DA when not trimmed
		status = deflateInit(&m_zs, Z_BEST_COMPRESSION);
		if (status != Z_OK)
			throw std::runtime_error("deflateInit failed");
	}

	~InflateWriter()
	{
		terminate();
		deflateEnd(&m_zs);
	}

	// Low-level raw file writing without file header and checksum
	void writeFile(const uint8_t *data, size_t len)
	{
		if (m_is_first) {
			m_is_first = false;

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

		//DEBUGLOG("zlib: write %zu bytes\n", len);
		m_file.write((const char *)data, len);
	}

	size_t write(const uint8_t *src, size_t n_bytes)
	{
		if (src && n_bytes == 0)
			return 0;

		// Stream dead. refuse.
		if (status != Z_OK)
			throw std::runtime_error("Stream ended. Cannot write more.");

		// To terminate the file
		uint8_t tmpbuf[10];
		if (!src) {
			src = tmpbuf;
			n_bytes = 0;
		}

		// https://www.zlib.net/zlib_how.html
		m_zs.next_in = (unsigned char *)src; // zlib might not be compiled with ZLIB_CONST
		m_zs.avail_in = n_bytes;
		size_t n_wrote = 0; // Total processed bytes

		do {
			m_zs.avail_out = CHUNK;
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
					ERRORLOG("zlib: error code %d, message: %s, near index 0x%04zX\n",
						status, m_zs.msg ? m_zs.msg : "NULL", getIndex());
					throw std::runtime_error("zlib: inflate error");
			}

			// The amount of newly received bytes
			size_t have = CHUNK - m_zs.avail_out;
			n_wrote = n_bytes - m_zs.avail_in;

			writeFile(m_buf_out, have);

			if (have > 0)
				DEBUGLOG("zlib: write %zu bytes\n", have);
			if (m_zs.avail_in == 0)
				break; // all bytes eaten

		} while (status != Z_STREAM_END);

		return n_wrote;
	}

	void terminate()
	{
		while (status == Z_OK)
			write(nullptr, 0);
	}

	// Same as be2le
	inline void le2be(u8 *be_dst, const u8 *le_src, size_t n_bytes)
	{
		for (size_t i = 0; i < n_bytes; ++i)
			be_dst[i] = le_src[(n_bytes - 1) - i];
	}

	// ---------------- Big endian writer functions ----------------
	template<typename T>
	void write(const T &val)
	{
		u8 ledata[sizeof(T)];
		u8 bedata[sizeof(T)];
		memcpy(ledata, &val, sizeof(T));
		le2be(bedata, ledata, sizeof(T));

		if (write(bedata, sizeof(T)) != sizeof(T))
			throw std::runtime_error("Failed to write");
	}

	void writeStr16(const std::string &str)
	{
		if (str.size() > UINT16_MAX)
			throw std::runtime_error("String too long to serialize");

		write<u16>(str.size());
		write((uint8_t *)str.c_str(), str.size());
	}

	int status;
	inline size_t getIndex() { return m_zs.total_in; }

private:
	std::ofstream m_file;

	z_stream m_zs;
	bool m_is_first = true;

	static constexpr size_t CHUNK = 4*1024;
	uint8_t m_buf_out[CHUNK]; // compressed data
};

struct DeflateReader {
	DeflateReader(const std::string &inputfile) :
		m_file(inputfile, std::ios::binary)
	{
		memset(&m_zs, 0, sizeof(m_zs));

		if (!m_file.good())
			throw std::runtime_error("Cannot open file: " + inputfile);

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
	size_t readFile(uint8_t *data, size_t len)
	{
		if (m_is_first) {
			m_is_first = false;

			// https://yal.cc/cs-deflatestream-zlib/
			data[0] = 0x78;
			data[1] = 0xDA; // Optimal

			// https://stackoverflow.com/questions/70347/zlib-compatible-compression-streams
			//data[0] = 0x78;
			//data[1] = 0x01;
			return 2;
		}

		if (m_file.eof()) {
			// Append missing checksum

			uint32_t to_write = m_zs.adler;
			for (int i = 0; i < 4; ++i)
				data[i] = ((const u8 *)&to_write)[3 - i];

			return 4;
		}

		// Sets failbit and eofbit on stream end
		m_file.read((char *)data, len);
		return m_file.gcount();
	}

	// Decompression
	size_t read(uint8_t *dst, size_t n_bytes)
	{
		if (n_bytes == 0)
			return 0;

		// Stream dead. refuse.
		if (status != Z_OK)
			throw std::runtime_error("Stream ended. Cannot read further.");

		// https://www.zlib.net/zlib_how.html
		size_t n_read = 0; // Total decompressed bytes
		do {
			if (m_zs.avail_in == 0) {
				// All bytes eaten. Read next block
				m_zs.avail_in = readFile(m_buf_in, CHUNK);
				DEBUGLOG("zlib: reading in %d bytes, space: %zu\n", m_zs.avail_in, n_bytes - n_read);

				if (m_file.bad())
					throw std::runtime_error("File read error");

				if (m_zs.avail_in == 0)
					throw std::runtime_error("File ended unexpectedly");

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
					//ERRORLOG("Got Adler %08lX\n", m_zs.adler);
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
			throw std::runtime_error("Failed to read @ index=" + std::to_string(getIndex()));

		T ret = 0;
		be2le((uint8_t *)&ret, bedata, sizeof(T));
		return ret;
	}

	std::string readStr16()
	{
		u16 len = read<u16>();
		std::string str(len, '\0');
		if (read((uint8_t *)&str[0], len) != len)
			throw std::runtime_error("Failed to read str16");
		return str;
	}

	std::vector<u16> readArrU16x32()
	{
		// Size is written in total bytes!
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
	std::ifstream m_file;

	z_stream m_zs;
	bool m_is_first = true;

	static constexpr size_t CHUNK = 4*1024;
	uint8_t m_buf_in[CHUNK]; // compressed data
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

static std::vector<bid_t> BLOCK_ID_LUT; // Translation table

static void fill_block_types()
{
	if (!BLOCK_TYPE_LUT.empty())
		return;

	BLOCK_TYPE_LUT.resize(2000);

	auto set_range = [](BlockDataType type, size_t first, size_t last) {
		while (first <= last) {
			BLOCK_TYPE_LUT.at(first) = type;
			first++;
		}
	};
	auto set_array = [](BlockDataType type, const u16 which[]) {
		while (*which) {
			BLOCK_TYPE_LUT.at(*which) = type;
			which++;
		}
	};

	// Based on https://github.com/capashaa/EEOEditor/blob/e8204a2acb/EELVL/Block.cs#L38-L85
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
		0 // Terminator
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

static void fill_block_translations()
{
	if (!BLOCK_ID_LUT.empty())
		return;

	BLOCK_ID_LUT.resize(1700); // default to "air"

	auto set_range = [](bid_t id, size_t first, size_t last) {
		while (first <= last) {
			BLOCK_ID_LUT.at(first) = id;
			first++;
		}
	};
	const bid_t SOLID = 9;

	set_range(SOLID, 37, 42); // beta
	set_range(SOLID, 17, 21); // brick
	set_range(SOLID, 34, 36); // metal
	set_range(SOLID, 51, 58); // glass
	set_range(SOLID, 70, 76); // minerals
	set_range(SOLID, 78, 82); // xmas11
	set_range(SOLID, 99, 104); // cowboy
	set_range(13, 137, 142); // sand -> yellow basic
	set_range(SOLID, 144, 153); // industrial
	set_range(SOLID, 158, 163); // medieval
	set_range(12, 166, 171); // orange pipes -> red basic
	set_range(SOLID, 172, 176); // space
	set_range(13, 177, 181); // desert -> yellow basic
	set_range(SOLID, 186, 192); // checker
	set_range(14, 193, 198); // jungle -> green basic
	set_range(12, 202, 204); // lava -> red basic
	set_range(SOLID, 208, 211); // marble

	set_range(SOLID, 1008, 1010); // gates (active)
	set_range(14, 1030, 1034); // nature -> green basic
	set_range(SOLID, 1035, 1040); // domestic)
	set_range(SOLID, 1047, 1049); // halloween15
	set_range(13, 1065, 1069); // gold -> yellow basic
	set_range(SOLID, 1059, 1063); // arctic
	set_range(SOLID, 1070, 1074); // fairytale
	set_range(SOLID, 1083, 1087); // summer16
	set_range(SOLID, 1096, 1100); // construction 1/2
	set_range(SOLID, 1128, 1131); // construction 2/2
	set_range(SOLID, 1106, 1115); // tile

	const bid_t solids[] = {
		// basic
		182,  // black
		1018, // orange
		1088, // white

		// beta
		1019, // blue
		1020, // orange
		1089, // white
		1021, // black

		// brick
		1022, // grey
		1023, // blue
		1024, // black
		1090, // white

		// special
		22, // construction
		32, // smiley
		33, // black rubber
		1057, // faceless 32
		1058, // inverted 22

		// checker
		1025,
		1026,
		1091,

		// gates
		157, // time
		206, // zombie
		214, // blue coin
		1008,
		1009,
		1010,
		1012, // death
		1095, // crown
		1153 // silver crown
	};
	for (bid_t id : solids)
		BLOCK_ID_LUT.at(id) = SOLID;

	BLOCK_ID_LUT[411] = 1; // invisible
	BLOCK_ID_LUT[412] = 2;
	BLOCK_ID_LUT[413] = 3;
	BLOCK_ID_LUT[414] = 4;

	const bid_t slow_climbable[] = {
		459, 460, // slow dot

		98, // vine V
		99, // vine H
		118, // chain
		120, // ninja
		424, // rope
		472, // fairytale
		1146, // vine
		1534, // metal
		1563, // stalk
		1602, // dungeon
	};
	for (bid_t id : slow_climbable)
		BLOCK_ID_LUT.at(id) = 4;
}

static void ensure_cache()
{
	fill_block_types();
	fill_block_translations();
}

static void read_eelvl_header(DeflateReader &zs, LobbyWorld &meta)
{
	meta.owner = zs.readStr16();
	meta.title = zs.readStr16();
	meta.size.X = zs.read<s32>();
	meta.size.Y = zs.read<s32>();
	zs.read<f32>(); // gravity
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

	DEBUGLOG("Importing (%d x %d) world by %s\n", meta.size.X, meta.size.Y, meta.owner.c_str());
	DEBUGLOG("\t Title: %s\n", meta.title.c_str());
	DEBUGLOG("\t Description: %s\n", description.c_str());
	DEBUGLOG("\t Crew: %s\n", crew_name.c_str());
}

void EEOconverter::fromFile(const std::string &filename_)
{
	std::filesystem::create_directories(IMPORT_DIR);
	const std::string filename = IMPORT_DIR + "/" + filename_;

	ensure_cache();

	DeflateReader zs(filename);

	// https://github.com/capashaa/EEOEditor/tree/main/EELVL

	LobbyWorld meta;
	dynamic_cast<IWorldMeta &>(meta) = dynamic_cast<IWorldMeta &>(m_world.getMeta());
	read_eelvl_header(zs, meta);
	dynamic_cast<IWorldMeta &>(m_world.getMeta()) = dynamic_cast<IWorldMeta &>(meta);

	// Actual block data comes now
	m_world.createEmpty(meta.size);

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
			// Attempt to remap unknown blocks
			if (layer == 0) {
				if ((size_t)block_id < BLOCK_ID_LUT.size())
					block_id = BLOCK_ID_LUT[block_id];
			} else {
				block_id = 0;
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

	printf("EEOconverter: Imported world %s from file '%s'\n", meta.id.c_str(), filename.c_str());
}

void EEOconverter::toFile(const std::string &filename_) const
{
	std::filesystem::create_directories(EXPORT_DIR);
	const std::string filename = EXPORT_DIR + "/" + filename_;

	ensure_cache();

	InflateWriter zs(filename);

	const WorldMeta &meta = m_world.getMeta();
	zs.writeStr16(meta.owner);
	zs.writeStr16(meta.title);
	const blockpos_t size = m_world.getSize();
	zs.write<s32>(size.X);
	zs.write<s32>(size.Y);
	zs.write<f32>(1); // gravity (default factor)
	zs.write<u32>(0); // bgcolor (default color)
	zs.writeStr16(""); // description
	zs.write<u8>(0); // campaign
	zs.writeStr16(""); // crew ID
	zs.writeStr16(""); // crew name
	zs.write<s32>(0); // crew status
	zs.write<u8>(1); // minimap
	zs.writeStr16("exported from OpenEdits"); // owner ID

	using posvec_t = std::vector<blockpos_t>;

	// List block positions per ID
	// TODO: What about coin doors, signs, portals?
	std::map<bid_t, posvec_t> fg_blocks, bg_blocks;
	blockpos_t pos;
	for (pos.Y = 0; pos.Y < size.Y; ++pos.Y)
	for (pos.X = 0; pos.X < size.X; ++pos.X) {
		Block b;
		m_world.getBlock(pos, &b);
		if (b.id) {
			auto &fg_posvec = fg_blocks[b.id];
			if (fg_posvec.empty())
				fg_posvec.reserve(200);
			fg_posvec.push_back(pos);
		}

		if (b.bg) {
			auto &bg_posvec = bg_blocks[b.bg];
			if (bg_posvec.empty())
				bg_posvec.reserve(200);
			bg_posvec.push_back(pos);
		}
	}

	// Write block vectors to the file
	auto write_array = [&](const posvec_t &posvec) {
		zs.write<u32>(posvec.size() * sizeof(u16));
		for (blockpos_t pos : posvec)
			zs.write<u16>(pos.X);

		zs.write<u32>(posvec.size() * sizeof(u16));
		for (blockpos_t pos : posvec)
			zs.write<u16>(pos.Y);
	};

	for (const auto &entry : fg_blocks) {
		zs.write<s32>(entry.first); // block ID
		zs.write<s32>(0); // layer
		write_array(entry.second);

		switch (BLOCK_TYPE_LUT[entry.first]) {
			case BlockDataType::None:
				break;
			case BlockDataType::I:
				zs.write<s32>(0);
				break;
			case BlockDataType::III:
				zs.write<s32>(0);
				zs.write<s32>(0);
				zs.write<s32>(0);
				break;
			case BlockDataType::SI:
				zs.writeStr16("");
				zs.write<s32>(0);
				break;
			case BlockDataType::SSI:
				zs.writeStr16("");
				zs.writeStr16("");
				zs.write<s32>(0);
				break;
			case BlockDataType::SSSS:
				zs.writeStr16("");
				zs.writeStr16("");
				zs.writeStr16("");
				zs.writeStr16("");
				break;
		}
	}

	for (const auto &entry : bg_blocks) {
		zs.write<s32>(entry.first); // block ID
		zs.write<s32>(1); // layer
		write_array(entry.second);
	}

	zs.terminate();

	printf("EEOconverter: Exported world %s to file '%s'\n", meta.id.c_str(), filename.c_str());
}


void EEOconverter::inflate(const std::string &filename)
{
	ensure_cache();

	DeflateReader zs(filename);

	const std::string outname(filename + ".inflated");
	std::ofstream of(outname, std::ios_base::binary);

	if (!of.good())
		throw std::runtime_error("Failed to open destination file");

	while (zs.status == Z_OK) {
		u8 buffer[4*1024];
		size_t read = zs.read(buffer, sizeof(buffer));
		of.write((const char *)buffer, read);
	}

	of.close();

	printf("EEOconverter: Decompressed file %s\n", outname.c_str());
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

static std::string path_to_worldid(const std::string &path)
{
	std::size_t hash = std::hash<std::string>()(path);
	char buf[100];
	snprintf(buf, sizeof(buf), "I%08x", (uint32_t)hash);
	return buf;
}

static bool is_path_ok(const std::filesystem::directory_entry &entry)
{
	if (!entry.is_regular_file())
		return false;

	for (const auto &part : entry.path()) {
		if (part.c_str()[0] == '.')
			return false;
	}

	if (entry.path().extension() != ".eelvl")
		return false;

	return true;
}

void EEOconverter::listImportableWorlds(std::map<std::string, LobbyWorld> &worlds)
{
	worlds.clear();

#ifdef HAVE_ZLIB
	if (0)
#endif
	{
		LobbyWorld dummy;
		dummy.id = "";
		dummy.title = "Feature unsupported by server";
		worlds[""] = dummy;
		return;
	}

	std::filesystem::create_directories(IMPORT_DIR);

#ifdef HAVE_ZLIB
	for (const auto &entry : std::filesystem::recursive_directory_iterator(IMPORT_DIR)) {
		if (!is_path_ok(entry))
			continue;

		LobbyWorld meta;
		try {
			DeflateReader zs(entry.path());
			read_eelvl_header(zs, meta);
		} catch (std::runtime_error &e) {
			DEBUGLOG("Cannot read '%s': %s\n", entry.path().c_str(), e.what());
			continue;
		}

		meta.id = path_to_worldid(entry.path());
		worlds[entry.path()] = meta;
	}
#endif
}

std::string EEOconverter::findWorldPath(const std::string &world_id)
{
#ifdef HAVE_ZLIB
	if (0)
#endif
	{
		return "";
	}

	std::filesystem::path root(IMPORT_DIR);
	for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
		if (!is_path_ok(entry))
			continue;

		if (world_id == path_to_worldid(entry.path())) {
			return std::filesystem::relative(entry.path(), root);
		}
	}

	return "";
}
