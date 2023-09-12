#include "world.h"
#include "blockmanager.h"
#include "macros.h"
#include "packet.h"
#include <cstring> // memset

bool BlockUpdate::set(bid_t block_id)
{
	auto props = m_mgr->getProps(block_id);
	if (!props) {
		id = Block::ID_INVALID;
		return false;
	}

	id = block_id | (BG_FLAG * props->isBackground());
	params = BlockParams(props->paramtypes);
	return true;
}

void BlockUpdate::setErase(bool background)
{
	id = 0 | (BG_FLAG * background);
	params = BlockParams();
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

WorldMeta::Type WorldMeta::idToType(const std::string &id)
{
	switch (id[0]) {
		case 'P': return WorldMeta::Type::Persistent;
		case 'T': return WorldMeta::Type::TmpDraw; // or TmpSimple
		case 'I': return WorldMeta::Type::Readonly;
	}
	return WorldMeta::Type::MAX_INVALID;
}


PlayerFlags WorldMeta::getPlayerFlags(const std::string &name) const
{
	if (name == owner)
		return PlayerFlags(PlayerFlags::PF_OWNER);

	auto it = player_flags.find(name);
	if (it != player_flags.end())
		return it->second;

	if (edit_code.empty()) {
		// Default permissions
		if (type == Type::TmpSimple)
			return PlayerFlags(PlayerFlags::PF_TMP_EDIT);
		if (type == Type::TmpDraw)
			return PlayerFlags(PlayerFlags::PF_TMP_EDIT_DRAW);
	}
	return PlayerFlags();
}

void WorldMeta::setPlayerFlags(const std::string &name, const PlayerFlags pf)
{
	player_flags[name] = pf;
}

void WorldMeta::readPlayerFlags(Packet &pkt)
{
	if (pkt.getRemainingBytes() == 0)
		return; // Manually created world

	u8 version = pkt.read<u8>();
	if (version < 4 || version > 5)
		throw std::runtime_error("Incompatible player flags version");

	player_flags.clear();

	if (version < 5)
		return;

	// Useful to enforce default flags in the future
	playerflags_t mask = pkt.read<playerflags_t>(); // known flags

	while (true) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break;

		PlayerFlags pf(0); // defaults
		playerflags_t flags = pkt.read<playerflags_t>();
		pf.flags |= flags & mask;

		player_flags.emplace(name, pf);
	}
}

void WorldMeta::writePlayerFlags(Packet &pkt) const
{
	pkt.write<u8>(5);
	pkt.write<playerflags_t>(PlayerFlags::PF_MASK_SAVE);

	for (auto it : player_flags) {
		if ((it.second.flags & PlayerFlags::PF_MASK_SAVE) == 0)
			continue;
		if (it.first == owner)
			continue;

		pkt.writeStr16(it.first);
		pkt.write<playerflags_t>(it.second.flags & PlayerFlags::PF_MASK_SAVE);
	}
	pkt.writeStr16(""); // end
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
	m_bmgr(bmgr),
	m_meta(new WorldMeta(id))
{
	m_meta->drop(); // Kept alive by RefCnt
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
		throw std::runtime_error("Already created");

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

void World::read(Packet &pkt, u16 protocol_version)
{
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
			readPlain(pkt, protocol_version);
			break;
		default:
			throw std::runtime_error("Unsupported world read method");
	}

	if (pkt.read<u16>() != VALIDATION)
		throw std::runtime_error("EOF validation mismatch");

	// good. done
}

void World::write(Packet &pkt, Method method, u16 protocol_version) const
{
	pkt.write<u32>(SIGNATURE);
	pkt.write((u8)method);


	/*
		Compatiblity solution: serialize the known param1 types
		so that the reader can discard unknown ones or use placeholders

		242:  portal_params = { INT16 }
		1000: text_params = { STRING }
	*/

	switch (method) {
		case Method::Dummy: break;
		case Method::Plain:
			writePlain(pkt, protocol_version);
			break;
		default:
			throw std::runtime_error("Unsupported world write method");
	}

	pkt.write<u16>(VALIDATION); // validity check
}

void World::readPlain(Packet &pkt, u16 protocol_version)
{
	u8 version = pkt.read<u8>();
	if (version < 2 || version > 4)
		throw std::runtime_error("Unsupported read version");

	// Describes the block parameters (thus length) that are to be expected
	std::map<bid_t, BlockParams::Type> mapper;
	if (protocol_version == PROTOCOL_VERSION_FAKE_DISK) {
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

void World::writePlain(Packet &pkt, u16 protocol_version) const
{
	u8 version = 4;
	// if (protocol_version < ??)
	//     version = 3;
	pkt.write(version);

	if (protocol_version == PROTOCOL_VERSION_FAKE_DISK) {
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
	for (size_t y = 0; y < m_size.Y; ++y)
	for (size_t x = 0; x < m_size.X; ++x) {
		blockpos_t pos(x, y);

		Block &b = getBlockRefNoCheck(pos);
		pkt.write(b.id);
		pkt.write(b.bg);

		auto props = m_bmgr->getProps(b.id);
		if (!props || props->paramtypes == BlockParams::Type::None)
			continue;

		// Write paramtype if there is any
		auto it = m_params.find(pos);
		if (it != m_params.end()) {
			if (it->second != props->paramtypes)
				throw std::runtime_error("Unexpected param format");
			it->second.write(pkt);
		} else {
			BlockParams params(props->paramtypes);
			params.write(pkt);
		}
	}
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

bool World::checkUpdateBlockNeeded(const BlockUpdate bu)
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

	if (new_id != ref.id)
		return true;

	if (bu.params != BlockParams::Type::None) {
		BlockParams params;
		return getParams(bu.pos, &params) && !(params == bu.params);
	}
	return false;
}

Block *World::updateBlock(const BlockUpdate bu)
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
