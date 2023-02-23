#include "world.h"
#include "blockmanager.h"
#include "macros.h"
#include "packet.h"
#include <cstring> // memset

bool BlockUpdate::set(bid_t block_id)
{
	auto props = g_blockmanager->getProps(block_id);
	if (!props) {
		id = Block::ID_INVALID;
		param1 = 0;
		return false;
	}

	bool background = props->tiles[0].type == BlockDrawType::Background;
	id = block_id | (BG_FLAG * background);
	param1 = 0;
	return true;
}

void BlockUpdate::setErase(bool background)
{
	id = 0 | (BG_FLAG * background);
	param1 = 0;
}

bool BlockUpdate::check(bid_t *block_id, bool *background, bool param1_check) const
{
	// For server-side validation
	bid_t id = getBlockId();
	auto props = g_blockmanager->getProps(id);
	if (!props)
		return false;

	// Special case: Always allow ID 0, but not others
	bool is_background = isBackground();
	if (id > 0 && (props->tiles[0].type == BlockDrawType::Background) != is_background)
		return false;

	if (param1_check && !is_background && !props->persistent_param1 && param1 != 0)
		return false;

	*block_id = id;
	*background = is_background;
	return true;
}

void BlockUpdate::read(Packet &pkt)
{
	pkt.read(pos.X);
	pkt.read(pos.Y);
	pkt.read(id);
	pkt.read(param1);
}

void BlockUpdate::write(Packet &pkt) const
{
	pkt.write(pos.X);
	pkt.write(pos.Y);
	pkt.write(id);
	pkt.write(param1);
}


void WorldMeta::readCommon(Packet &pkt)
{
	title = pkt.readStr16();
	owner = pkt.readStr16();
	online = pkt.read<u16>();
	plays = pkt.read<u32>();
}

void WorldMeta::writeCommon(Packet &pkt) const
{
	pkt.writeStr16(title);
	pkt.writeStr16(owner);
	pkt.write<u16>(online);
	pkt.write<u32>(plays);
}

PlayerFlags WorldMeta::getPlayerFlags(const std::string &name) const
{
	if (name == owner)
		return PlayerFlags(PlayerFlags::PF_OWNER);

	auto it = player_flags.find(name);
	return it != player_flags.end() ? it->second : PlayerFlags();
}

void WorldMeta::setPlayerFlags(const std::string &name, const PlayerFlags pf)
{
	player_flags.emplace(name, pf);
}

void WorldMeta::readPlayerFlags(Packet &pkt)
{
	u8 version = pkt.read<u8>();
	if (version < 4 || version > 5)
		throw std::runtime_error("Incompatible player flags version");

	player_flags.clear();

	if (version == 4)
		return;

	// Useful to enforce default flags in the future
	pkt.read<playerflags_t>(); // known flags

	while (true) {
		std::string name = pkt.readStr16();
		if (name.empty())
			break;

		PlayerFlags pf;
		pkt.read(pf.flags);

		player_flags.emplace(name, pf);
	}
}

void WorldMeta::writePlayerFlags(Packet &pkt) const
{
	pkt.write<u8>(5);
	pkt.write<playerflags_t>(PlayerFlags::PF_MASK_SAVE);

	for (auto it : player_flags) {
		if (!it.second.check(PlayerFlags::PF_MASK_SAVE))
			continue;
		if (it.first == owner)
			continue;

		pkt.writeStr16(it.first);
		pkt.write<playerflags_t>(it.second.flags & PlayerFlags::PF_MASK_SAVE);
	}
	pkt.writeStr16(""); // end
}

bool WorldMeta::Key::trigger(float refill)
{
	bool changed = !active;

	if (refill < 0) {
		// No re-filling if active
		if (!active)
			cooldown = -refill;
	} else {
		// Refill regardless
		cooldown = refill;
	}
	active = true;
	return changed;
}


bool WorldMeta::Key::step(float dtime)
{
	if (cooldown > 0)
		cooldown -= dtime;
	if (cooldown <= 0 && active) {
		active = false;
		return true;
	}
	return false;
}


// -------------- World class -------------

World::World(const std::string &id) :
	m_meta(id)
{
	ASSERT_FORCED(g_blockmanager, "BlockManager is required");
	printf("World: Create %s\n", id.c_str());
}

World::~World()
{
	printf("World: Delete %s\n", m_meta.id.c_str());
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

void World::read(Packet &pkt)
{
	if (m_size.X == 0 || m_size.Y == 0)
		throw std::runtime_error("World size error (not initialized?)");
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
			writePlain(pkt);
			break;
		default:
			throw std::runtime_error("Unsupported world write method");
	}

	pkt.write<u16>(VALIDATION); // validity check
}

void World::readPlain(Packet &pkt)
{
	u8 version = pkt.read<u8>();
	if (version < 2 || version > 3)
		throw std::runtime_error("Unsupported read version");

	for (size_t y = 0; y < m_size.Y; ++y)
	for (size_t x = 0; x < m_size.X; ++x) {
		Block b;
		pkt.read(b.id);
		pkt.read(b.bg);
		if (version >= 3)
			pkt.read(b.param1);
		getBlockRefNoCheck(blockpos_t(x, y)) = b;
	}
}

void World::writePlain(Packet &pkt) const
{
	pkt.write<u8>(3); // version

	pkt.ensureCapacity(m_size.X * m_size.Y * sizeof(Block));
	for (size_t y = 0; y < m_size.Y; ++y)
	for (size_t x = 0; x < m_size.X; ++x) {
		Block &b = getBlockRefNoCheck(blockpos_t(x, y));
		pkt.write(b.id);
		pkt.write(b.bg);
		pkt.write(b.param1);
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

	Block &ref = getBlockRefNoCheck(pos);
	if (ref == block)
		return false;

	ref = block;
	return true;
}

bool World::updateBlock(const BlockUpdate bu, bool param1_check)
{
	if (bu.pos.X >= m_size.X || bu.pos.Y >= m_size.Y)
		return false;

	bid_t new_id;
	bool is_background;
	if (!bu.check(&new_id, &is_background, param1_check))
		return false;

	Block &ref = getBlockRefNoCheck(bu.pos);
	if (is_background) {
		if (new_id == ref.bg)
			return false;

		ref.bg = new_id;
	} else {
		if (new_id == ref.id && bu.param1 == ref.param1)
			return false;

		ref.id = new_id;
		ref.param1 = bu.param1;
	}

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
