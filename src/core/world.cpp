#include "world.h"
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
	m_data = new Block[length]; // *2

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
