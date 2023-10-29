#include "eeo_converter.h"
#include <stdexcept>

#include "core/blockmanager.h"
#include "core/compressor.h"
#include "core/packet.h"
#include "core/world.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <string.h> // memcpy

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) do {} while (false)
#endif
#define ERRORLOG(...) fprintf(stderr, __VA_ARGS__)


const std::string EEOconverter::IMPORT_DIR = "worlds/imports";
const std::string EEOconverter::EXPORT_DIR = "worlds/exports";

struct EBlockParams {
	enum class Type {
		None,
		I, // Rotation, Number, etc
		III, // Portal
		SI, // Sign, World portal
		SSI, // Label
		SSSS // NPC ??
	};

	s32 val_I[3] = { 0 };
	//std::string val_S[4] = { };

	// --------- EBlockParams -> BlockParams conversion ---------

	#define PARAM_CONV_IMPORT_REG(name) \
		void (name)(bid_t id, BlockParams &params)
	typedef PARAM_CONV_IMPORT_REG(EBlockParams::*ConvImporter);

	static void registerImports();
	static std::map<bid_t, ConvImporter> conv_import;

	bool importParams(bid_t id, BlockParams &params);

	// BlockParams is initialized by BlockUpdate::set() based on BlockProperties::paramtypes
	PARAM_CONV_IMPORT_REG(importSpike)
	{
		params.param_u8 = (val_I[0] + 3) % 4;
	}

	PARAM_CONV_IMPORT_REG(importCoindoor)
	{
		params.param_u8 = val_I[0];
	}

	PARAM_CONV_IMPORT_REG(importTeleporter)
	{
		params.teleporter.rotation = val_I[0];
		params.teleporter.id = val_I[1];
		params.teleporter.dst_id = val_I[2];
	}
};

std::map<bid_t, EBlockParams::ConvImporter> EBlockParams::conv_import;

static std::vector<EBlockParams::Type> BLOCK_TYPE_LUT; // Indexed by block ID

static std::vector<bid_t> BLOCK_ID_LUT; // Translation table

static void fill_block_types()
{
	if (!BLOCK_TYPE_LUT.empty())
		return;

	using PType = EBlockParams::Type;
	BLOCK_TYPE_LUT.resize(2000);

	auto set_range = [](PType type, size_t first, size_t last) {
		while (first <= last) {
			BLOCK_TYPE_LUT.at(first) = type;
			first++;
		}
	};
	auto set_array = [](PType type, const u16 which[]) {
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
	set_array(PType::I, types_I);

	static const u16 types_III[] = { 381, 242, 0 };
	set_array(PType::III, types_III);

	BLOCK_TYPE_LUT.at(374) = PType::SI;
	BLOCK_TYPE_LUT.at(385) = PType::SI;

	BLOCK_TYPE_LUT.at(1000) = PType::SSI;

	set_range(PType::SSSS, 1550, 1559);
	set_range(PType::SSSS, 1569, 1579);
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

	set_range(Block::ID_SPIKES, 1625, 1636); // spikes, every 2nd is not rotatable
	BLOCK_ID_LUT.at(1580) = Block::ID_SPIKES; // not rotatable
	BLOCK_ID_LUT.at(381) = Block::ID_TELEPORTER; // invisible teleporter
}

void EBlockParams::registerImports()
{
	for (bid_t id : { 1625, 1627, 1629, 1631, 1633, 1635 })
		conv_import[id] = &EBlockParams::importSpike;
	for (bid_t id : { Block::ID_COINDOOR, Block::ID_COINGATE })
		conv_import[id] = &EBlockParams::importCoindoor;
	conv_import[Block::ID_TELEPORTER] = &EBlockParams::importTeleporter;
}

bool EBlockParams::importParams(bid_t id, BlockParams &params)
{
	auto it = conv_import.find(id);
	if (it == conv_import.end())
		return false;

	(this->*it->second)(id, params);
	return true;
}

static void ensure_cache()
{
	fill_block_types();
	fill_block_translations();
	EBlockParams::registerImports();
}

static void read_eelvl_header(Packet &zs, LobbyWorld &meta)
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

static std::vector<u16> readArrU16x32(Packet &pkt)
{
	// Size is written in total bytes!
	u32 len = pkt.read<u32>() / sizeof(u16); // bytes -> count

	//DEBUGLOG("Array size: %u at index 0x%04zX\n", len, getIndex());
	std::vector<u16> ret;
	ret.resize(len);
	for (size_t i = 0; i < len; ++i)
		ret[i] = pkt.read<u16>();
	return ret;
}

static void decompress_file(Packet &pkt, const std::string &filename)
{
	std::ifstream is(filename, std::ios_base::binary);
	if (!is.good())
		throw std::runtime_error("Cannot read open file " + filename);

	is.seekg(0, is.end);
	size_t length = is.tellg();
	is.seekg(0, is.beg);
	Packet pkt_zlib(length);

	DEBUGLOG("Decompress file '%s', len=%zu\n", filename.c_str(), length);
	do {
		is.read((char *)pkt_zlib.writePreallocStart(1000), 1000);
		pkt_zlib.writePreallocEnd(is.gcount()); // already written
	} while (is.good());

	pkt.ensureCapacity((length * 10) / 2); // approx. decompressed size / 2
	Decompressor decomp(&pkt, pkt_zlib);
	decomp.setBarebone();
	decomp.decompress();

	pkt.setBigEndian(); // for reading
}

void EEOconverter::fromFile(const std::string &filename_)
{
	std::filesystem::create_directories(IMPORT_DIR);
	const std::string filename = IMPORT_DIR + "/" + filename_;

	ensure_cache();

	Packet zs;
	decompress_file(zs, filename);

	// https://github.com/capashaa/EEOEditor/tree/main/EELVL

	LobbyWorld meta;
	dynamic_cast<IWorldMeta &>(meta) = dynamic_cast<IWorldMeta &>(m_world.getMeta());
	read_eelvl_header(zs, meta);
	dynamic_cast<IWorldMeta &>(m_world.getMeta()) = dynamic_cast<IWorldMeta &>(meta);

	// Actual block data comes now
	m_world.createEmpty(meta.size);

	auto blockmgr = m_world.getBlockMgr();

	std::string err;
	EBlockParams params_in;
	BlockUpdate bu(blockmgr);
	while (zs.getRemainingBytes() > 0) {
		int block_id;
		try {
			block_id = zs.read<s32>();
		} catch (std::exception &e) {
			err = e.what();
			break;
		}

		int layer = zs.read<s32>();
		DEBUGLOG("\n--> Next block ID: %4d, layer=%d, offset=0x%04zX\n", block_id, layer, zs.getReadPos());
		if (layer < 0 || layer > 1)
			throw std::runtime_error("Previous block data mismatch");

		auto pos_x = readArrU16x32(zs);
		auto pos_y = readArrU16x32(zs);

		using PType = EBlockParams::Type;
		PType type = BLOCK_TYPE_LUT[block_id];
		DEBUGLOG("    Data: count=%zu, type=%d, offset=0x%04zX\n",
			pos_x.size(), (int)type, zs.getReadPos());

		if (!bu.set(block_id)) {
			// Attempt to remap unknown blocks
			if (layer == 0) {
				if ((size_t)block_id < BLOCK_ID_LUT.size()) {
					bu.set(BLOCK_ID_LUT[block_id]);
				}
			}
		}

		switch (type) {
			case PType::None: break;
			case PType::I:
				params_in.val_I[0] = zs.read<s32>();
				params_in.importParams(block_id, bu.params);
				break;
			case PType::III:
				params_in.val_I[0] = zs.read<s32>();
				params_in.val_I[1] = zs.read<s32>();
				params_in.val_I[2] = zs.read<s32>();
				params_in.importParams(block_id, bu.params);
				break;
			case PType::SI:
				zs.readStr16();
				zs.read<s32>();
				break;
			case PType::SSI:
				zs.readStr16();
				zs.readStr16();
				zs.read<s32>();
				break;
			case PType::SSSS:
				zs.readStr16();
				zs.readStr16();
				zs.readStr16();
				zs.readStr16();
				break;
		}

		if (bu.getId() == 0 || bu.getId() == Block::ID_INVALID)
			continue; // do not add these blocks to the map

		for (size_t i = 0; i < pos_x.size(); ++i) {
			bu.pos.X = pos_x[i];
			bu.pos.Y = pos_y[i];
			m_world.updateBlock(bu);
		}
	}

	printf("EEOconverter: Imported world %s from file '%s'\n", meta.id.c_str(), filename.c_str());
}

void EEOconverter::toFile(const std::string &filename_) const
{
	std::filesystem::create_directories(EXPORT_DIR);
	const std::string filename = EXPORT_DIR + "/" + filename_;

	ensure_cache();

	std::ofstream os(filename, std::ios_base::binary);
	if (!os.good())
		throw std::runtime_error("Cannot write open file " + filename);

	Packet zs;
	zs.setBigEndian();

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

	using PType = EBlockParams::Type;
	for (const auto &entry : fg_blocks) {
		zs.write<s32>(entry.first); // block ID
		zs.write<s32>(0); // layer
		write_array(entry.second);

		switch (BLOCK_TYPE_LUT[entry.first]) {
			case PType::None:
				break;
			case PType::I:
				zs.write<s32>(0);
				break;
			case PType::III:
				zs.write<s32>(0);
				zs.write<s32>(0);
				zs.write<s32>(0);
				break;
			case PType::SI:
				zs.writeStr16("");
				zs.write<s32>(0);
				break;
			case PType::SSI:
				zs.writeStr16("");
				zs.writeStr16("");
				zs.write<s32>(0);
				break;
			case PType::SSSS:
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

	{
		Packet pkt_zlib;
		Compressor comp(&pkt_zlib, zs);
		comp.setBarebone();
		comp.compress();
		os.write((const char *)pkt_zlib.data(), pkt_zlib.size());
		os.close();
	}

	printf("EEOconverter: Exported world %s to file '%s'\n", meta.id.c_str(), filename.c_str());
}


void EEOconverter::inflate(const std::string &filename)
{
	ensure_cache();

	const std::string outname(filename + ".inflated");
	std::ofstream of(outname, std::ios_base::binary);

	if (!of.good())
		throw std::runtime_error("Failed to open destination file");

	Packet zs;
	decompress_file(zs, filename);

	of.write((const char *)zs.data(), zs.size());
	of.close();

	printf("EEOconverter: Decompressed file %s\n", outname.c_str());
}

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

		std::string path = entry.path().u8string();
		LobbyWorld meta;
		try {
			Packet pkt;
			decompress_file(pkt, path);
			read_eelvl_header(pkt, meta);
		} catch (std::runtime_error &e) {
			DEBUGLOG("Cannot read '%s': %s\n", path.c_str(), e.what());
			continue;
		}

		meta.id = path_to_worldid(path);
		worlds[path] = meta;
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

		if (world_id == path_to_worldid(entry.path().u8string()))
			return std::filesystem::relative(entry.path(), root).u8string();
	}

	return "";
}
