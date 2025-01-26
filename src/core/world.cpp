#include "world.h"
#include "blockmanager.h"
#include "compressor.h"
#include "macros.h"
#include "operators.h" // PositionRange
#include "packet.h"
#include "utils.h" // strtrim
#include "worldmeta.h"
#include <cstring> // memset

bool BlockUpdate::set(bid_t block_id)
{
	id = block_id;

	auto props = m_mgr->getProps(getId());
	if (!props) {
		id = Block::ID_INVALID;
		return false;
	}

	id = block_id | (BG_FLAG * props->isBackground());
	params = BlockParams(props->paramtypes);
	return true;
}

bool BlockUpdate::check(bid_t *block_id, bool *is_bg) const
{
	// For server-side validation
	bid_t id = getId();
	auto props = m_mgr->getProps(id);
	if (!props)
		return false;

	// Special case: Always allow ID 0, but not others
	bool is_background = isBackground();
	if (id > 0 && props->isBackground() != is_background)
		return false;

	if (params != props->paramtypes)
		return false;

	*block_id = id;
	*is_bg = is_background;
	return true;
}

bool BlockUpdate::sanitizeParams()
{
	switch (getId()) {
		case Block::ID_TEXT: {
			constexpr size_t LEN_MAX = 50;

			std::string &text = *params.text;
			text = strtrim(text);
			if (text.empty())
				return false;

			if (text.size() > LEN_MAX)
				text.resize(LEN_MAX);
		}
		break;
	}
	return true;
}


void BlockUpdate::read(Packet &pkt)
{
	pkt.read(pos.X);
	pkt.read(pos.Y);
	pkt.read(id);
	auto props = m_mgr->getProps(getId());
	if (!props)
		throw std::runtime_error("Unknown block ID");

	params = BlockParams(props->paramtypes);
	params.read(pkt);
}

void BlockUpdate::write(Packet &pkt) const
{
	if (id == Block::ID_INVALID)
		throw std::runtime_error("Uninitialized BlockUpdate");

	pkt.write(pos.X);
	pkt.write(pos.Y);
	pkt.write(id);
	params.write(pkt);
}

// Used for network only!
void IWorldMeta::readCommon(Packet &pkt)
{
	uint8_t version = pkt.read<uint8_t>();
	if (version < 1) {
		fprintf(stderr, "Invalid IWorldMeta version: %i\n", (int)version);
		return;
	}

	id = pkt.readStr16();
	title = pkt.readStr16();
	owner = pkt.readStr16();
	online = pkt.read<u16>();
	plays = pkt.read<u32>();
}

void IWorldMeta::writeCommon(Packet &pkt) const
{
	pkt.write<uint8_t>(1);
	pkt.writeStr16(id);
	pkt.writeStr16(title);
	pkt.writeStr16(owner);
	pkt.write<u16>(online);
	pkt.write<u32>(plays);
}

// -------------- World class -------------

World::World(World *copy_from) :
	m_bmgr(copy_from->m_bmgr),
	m_meta(copy_from->m_meta)
{
	ASSERT_FORCED(m_bmgr, "BlockManager is required");
	printf("World: Create %s\n", m_meta->id.c_str());
}

World::World(const BlockManager *bmgr, const std::string &id) :
	m_bmgr(bmgr)
{
	m_meta = std::make_shared<WorldMeta>(id);
	printf("World: Create %s\n", m_meta->id.c_str());
}

World::~World()
{
	printf("World: Delete %s\n", m_meta->id.c_str());
	delete[] m_data;
}

void World::createEmpty(blockpos_t size)
{
	if (size.X == 0 || size.Y == 0)
		throw std::length_error("Invalid size");
	if (m_data)
		delete[] m_data;

	m_size = size;

	const size_t length = m_size.X * m_size.Y;
	m_data = new Block[length];

	// Prevents the warning -Wclass-memaccess
	memset((void *)m_data, 0, length);
}

void World::createDummy(blockpos_t size)
{
	createEmpty(size);

	for (u16 y = m_size.Y / 2; y < (u16)m_size.Y; ++y)
	for (u16 x = 0; x < (u16)m_size.X; ++x) {
		getBlockRefNoCheck({x, y}).id = 9;
	}
}

static constexpr u32 SIGNATURE = 0x6677454F; // OEwf
static constexpr u16 VALIDATION = 0x4B4F; // OK

void World::read(Packet &pkt)
{
	ASSERT_FORCED(pkt.data_version != 0, "invalid proto ver");

	if (m_size.X == 0 || m_size.Y == 0)
		throw std::runtime_error("World size error (not initialized?)");
	if (pkt.getRemainingBytes() == 0)
		return; // Manually created world
	if (pkt.read<u32>() != SIGNATURE)
		throw std::runtime_error("World signature mismatch");

	Method method = (Method)pkt.read<u8>();

	switch (method) {
		case Method::Dummy: break;
		case Method::Plain:
			readPlain(pkt);
			break;
		default:
			throw std::runtime_error("Unsupported world read method");
	}

	if (pkt.read<u16>() != VALIDATION)
		throw std::runtime_error("EOF validation mismatch");

	// good. done
}

void World::write(Packet &pkt, Method method) const
{
	ASSERT_FORCED(pkt.data_version != 0, "Invalid proto ver");

	pkt.write<u32>(SIGNATURE);
	pkt.write((u8)method);

	switch (method) {
		case Method::Dummy: break;
		case Method::Plain:
			writePlain(pkt);
			break;
		default:
			throw std::runtime_error("Unsupported world write method");
	}

	pkt.write<u16>(VALIDATION); // validity check
}

void World::readPlain(Packet &pkt_in)
{
	u8 version = pkt_in.read<u8>();
	if (version < 2 || version > 5)
		throw std::runtime_error("Unsupported read version");

	const bool is_compressed = version >= 5;
	Packet pkt_tmp_decomp;
	Packet &pkt = is_compressed ? pkt_tmp_decomp : pkt_in;
	if (is_compressed) {
		Decompressor d(&pkt, pkt_in);
		// Simple 500 * 500 worlds are about 1 MB (decompressed)
		d.setLimit(10 * (1024 * 1024)); // 10 MiB must suffice
		d.decompress();
	}

	// Describes the block parameters (thus length) that are to be expected
	std::map<bid_t, BlockParams::Type> mapper;
	if (pkt_in.data_version == PROTOCOL_VERSION_FAKE_DISK) {
		// Load params from the disk
		if (version >= 4) {
			while (true) {
				bid_t id = pkt.read<bid_t>();
				if (!id)
					break;

				mapper.emplace(id, (BlockParams::Type)pkt.read<uint8_t>());
			}
		}
	} else {
		// In sync with the server, sent params upon connect
		const auto &props = m_bmgr->getProps();

		for (size_t i = 0; i < props.size(); ++i) {
			if (!props[i] || props[i]->paramtypes == BlockParams::Type::None)
				continue;

			mapper.emplace(i, props[i]->paramtypes);
		}
	}

	m_params.clear();
	for (size_t y = 0; y < m_size.Y; ++y)
	for (size_t x = 0; x < m_size.X; ++x) {
		blockpos_t pos(x, y);

		Block b;
		// Discard tile information (0 is always the default)
		b.id = pkt.read<bid_t>();
		b.bg = pkt.read<bid_t>();

		if (version == 3)
			pkt.read<u8>(); // param1

		if (version >= 4) {
			BlockParams val;
			auto it = mapper.find(b.id);
			if (it != mapper.end()) {
				val = BlockParams(it->second);
				val.read(pkt);
			}

			if (val != BlockParams::Type::None) {
				auto props = m_bmgr->getProps(b.id);
				if (props && val == props->paramtypes)
					m_params.emplace(pos, val);
			}
		}

		getBlockRefNoCheck(pos) = b;
	}
}

void World::writePlain(Packet &pkt_out) const
{
	u8 version = 5;
	//if (pkt_out.data_version >= 7)
	//	version = 6;
	pkt_out.write(version);

	const bool do_compress = version >= 5;
	Packet pkt_tmp_comp;
	Packet &pkt = do_compress ? pkt_tmp_comp : pkt_out;

	if (pkt_out.data_version == PROTOCOL_VERSION_FAKE_DISK) {
		// Mapping of the known types
		auto &list = m_bmgr->getProps();
		for (size_t i = 0; i < list.size(); ++i) {
			auto props = list[i];
			if (!props || props->paramtypes == BlockParams::Type::None)
				continue;

			pkt.write<bid_t>(i);
			pkt.write<u8>((u8)props->paramtypes);
		}
		pkt.write<bid_t>(0); // terminator
	}

	pkt.ensureCapacity(m_size.X * m_size.Y * sizeof(Block));

	// Compressing backgrounds separate can result in 5-8% smaller files.
	// Busy worlds however benefit more from FG + BG in combination

	for (const Block *b = begin(); b != end(); ++b) {
		pkt.write(b->id);
		pkt.write(b->bg);

		auto props = m_bmgr->getProps(b->id);
		if (!props || props->paramtypes == BlockParams::Type::None)
			continue;

		blockpos_t pos = getBlockPos(b);

		// Write paramtype if there is any
		auto it = m_params.find(pos);
		if (it != m_params.end()) {
			if (it->second != props->paramtypes)
				throw std::runtime_error("Unexpected param format");
			it->second.write(pkt);
		} else {
			// Placeholder in case of missing data
			BlockParams params(props->paramtypes);
			params.write(pkt);
		}
	}

	if (do_compress) {
		Compressor c(&pkt_out, pkt);
		c.compress();
	}
}

Block *World::getBlockPtr(blockpos_t pos) const
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y)
		return nullptr;

	return &getBlockRefNoCheck(pos);
}

bool World::getBlock(blockpos_t pos, Block *block) const
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y)
		return false;

	if (block)
		*block = getBlockRefNoCheck(pos);

	return true;
}

bool World::setBlock(blockpos_t pos, const Block block)
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y)
		return false;

	getBlockRefNoCheck(pos) = block;
	return true;
}

blockpos_t World::getBlockPos(const Block *b) const
{
	if (b < begin() || b >= end())
		return blockpos_t(BLOCKPOS_INVALID, BLOCKPOS_INVALID);

	size_t index = b - begin();
	blockpos_t pos;
	pos.Y = index / m_size.X;
	pos.X = index - (pos.Y * m_size.X);
	return pos;
}

bool World::checkUpdateBlockNeeded(BlockUpdate &bu)
{
	if (bu.pos.X >= m_size.X || bu.pos.Y >= m_size.Y)
		return false;

	bid_t new_id;
	bool is_background;
	if (!bu.check(&new_id, &is_background))
		return false;

	Block &ref = getBlockRefNoCheck(bu.pos);
	if (is_background)
		return new_id != ref.bg;

	if (!bu.sanitizeParams())
		return false;

	if (new_id != ref.id)
		return true;

	if (bu.params != BlockParams::Type::None) {
		BlockParams params;
		return getParams(bu.pos, &params) && !(params == bu.params);
	}
	return false;
}

Block *World::updateBlock(BlockUpdate bu)
{
	if (!checkUpdateBlockNeeded(bu))
		return nullptr;

	Block &ref = getBlockRefNoCheck(bu.pos);
	if (bu.isBackground()) {
		ref.bg = bu.getId();
	} else {
		m_params.erase(bu.pos);
		ref.tile = 0;
		ref.id = bu.getId(); // reset tile information
		if (bu.params != BlockParams::Type::None)
			m_params.emplace(bu.pos, bu.params);
	}

	return &ref;
}

bool World::setBlockTiles(PositionRange &range, bid_t block_id, int tile)
{
	using O = PositionRange::Operator;

	const O op = range.op;
	int sum;

	bool modified = false;
	blockpos_t pos;

	if (range.type == PositionRange::PRT_ENTIRE_WORLD && op == O::PROP_SET) {
		// Optimization
		for (Block *b = begin(); b != end(); ++b) {
			if (b->id == block_id) {
				b->tile = tile;
				modified = true;
			}
		}
		goto done;
	}

	for (bool ok = range.iteratorStart(this, &pos); ok; ok = range.iteratorNext(&pos)) {
		Block &b = getBlockRefNoCheck(pos);
		if (b.id != block_id)
			continue;

		switch (op) {
			case O::PROP_SET:
				b.tile = tile;
				break;
			case O::PROP_ADD:
				// Disallow underflows
				// Issue: (255 + N) becomes 0 too.
				sum = tile + (int)b.tile;
				b.tile = (sum == (int)(uint8_t)sum) * sum;
				break;
			default:
				goto done; // invalid
		}
		modified = true;
	}

done:
	this->was_modified |= modified;
	return modified;
}


const BlockParams *World::getParamsPtr(blockpos_t pos) const
{
	auto it = m_params.find(pos);
	if (it == m_params.end())
		return nullptr;

	return &it->second;
}


bool World::getParams(blockpos_t pos, BlockParams *params) const
{
	auto it = m_params.find(pos);
	if (it == m_params.end())
		return false;

	*params = it->second;
	return true;
}


std::vector<blockpos_t> World::getBlocks(bid_t block_id, std::function<bool(Block &b)> callback) const
{
	std::vector<blockpos_t> found;
	found.reserve(std::hypot(m_size.X, m_size.Y) * 2);

	for (size_t y = 0; y < m_size.Y; ++y)
	for (size_t x = 0; x < m_size.X; ++x) {
		blockpos_t pos(x, y);
		Block &b = getBlockRefNoCheck(pos);
		if (b.id == block_id) {
			if (!callback || callback(b))
				found.emplace_back(pos);
		}
	}

	return found;
}
