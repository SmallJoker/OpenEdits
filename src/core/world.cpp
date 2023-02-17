#include "world.h"
#include "blockmanager.h"
#include "macros.h"
#include "packet.h"
#include <cstring> // memset

void WorldMeta::readCommon(Packet &pkt)
{
	title = pkt.readStr16();
	owner = pkt.readStr16();
	online = pkt.read<u16>();
	plays = pkt.read<u32>();
}

void WorldMeta::writeCommon(Packet &pkt)
{
	pkt.writeStr16(title);
	pkt.writeStr16(owner);
	pkt.write<u16>(online);
	pkt.write<u32>(plays);
}

const playerflags_t WorldMeta::getPlayerFlags(const std::string &name) const
{
	auto it = player_flags.find(name);
	return it != player_flags.end() ? it->second : 0;
}

void WorldMeta::readPlayerFlags(Packet &pkt)
{
	u8 version = pkt.read<u8>();
	if (version > 4 || version < 4)
		throw std::runtime_error("Incompatible player flags version");

	player_flags.clear();

	while (true) {
		bool is_ok = pkt.read<u8>();
		if (!is_ok)
			break;

		std::string name = pkt.readStr16();
		playerflags_t flags = pkt.read<playerflags_t>();

		player_flags.emplace(name, flags);
	}
}

void WorldMeta::writePlayerFlags(Packet &pkt) const
{
	pkt.write<u8>(4);

	for (auto it : player_flags) {
		if (it.second == 0)
			continue;

		pkt.write<u8>(true); // begin

		pkt.writeStr16(it.first);
		pkt.write(it.second);

	}
	pkt.write<u8>(false); // end
}


// -------------- World class -------------

World::World(const std::string &id) :
	m_meta({ .id = id })
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
	if (version != 2)
		throw std::runtime_error("Unsupported read version");

	for (size_t y = 0; y < m_size.Y; ++y)
	for (size_t x = 0; x < m_size.X; ++x) {
		Block b;
		pkt.read(b.id);
		pkt.read(b.bg);
		getBlockRefNoCheck(blockpos_t(x, y)) = b;
	}
}

void World::writePlain(Packet &pkt) const
{
	pkt.write<u8>(2); // version

	pkt.ensureCapacity(m_size.X * m_size.Y * sizeof(Block));
	for (size_t y = 0; y < m_size.Y; ++y)
	for (size_t x = 0; x < m_size.X; ++x) {
		Block &b = getBlockRefNoCheck(blockpos_t(x, y));
		pkt.write(b.id);
		pkt.write(b.bg);
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

bool World::updateBlock(const BlockUpdate bu)
{
	if (bu.pos.X >= m_size.X || bu.pos.Y >= m_size.Y)
		return false;

	bid_t new_id = bu.id & ~BlockUpdate::BG_FLAG;
	auto props = g_blockmanager->getProps(new_id);
	if (!props)
		return false;

	// Special case: Always allow ID 0, but not others
	bool is_background = (bu.id & BlockUpdate::BG_FLAG) > 0;
	if (new_id > 0 && (props->type == BlockDrawType::Background) != is_background)
		return false;

	Block &ref = getBlockRefNoCheck(bu.pos);
	bid_t &id_ref = is_background ? ref.bg : ref.id;

	if (id_ref == new_id)
		return false;

	id_ref = new_id;
	return true;
}
