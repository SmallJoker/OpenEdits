#include "world.h"
#include "macros.h"
#include <cstring> // memset

World::World()
{
	ASSERT_FORCED(g_blockmanager, "BlockManager is required");
}

World::~World()
{
	delete[] m_data;
}

void World::createEmpty(blockpos_t size)
{
	if (size.X == 0 || size.Y == 0)
		throw std::length_error("Invalid size");
	if (m_data)
		throw std::runtime_error("Already created");

	m_size = size;

	const size_t length = m_size.X * m_size.Y * 2;
	m_data = new Block[length]; // *2

	memset(m_data, 0, length);
}

void World::createDummy(blockpos_t size)
{
	createEmpty(size);

	for (u16 y = m_size.Y / 2; y < (u16)m_size.Y; ++y)
	for (u16 x = 0; x < (u16)m_size.X; ++x) {
		getBlockRefNoCheck({x, y}, 0).id = 9;
	}
}

bool World::getBlock(blockpos_t pos, Block *block, char layer) const
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y || layer > 1)
		return false;

	if (block)
		*block = getBlockRefNoCheck(pos, layer);

	return true;
}

bool World::setBlock(blockpos_t pos, Block block, char layer)
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y || layer > 1)
		return false;

	auto props = g_blockmanager->getProps(block.id);
	if (!props)
		return false;

	// backgrounds only on layer 1 and foreground on layer 0
	if ((props->type == BlockDrawType::Background) != layer)
		return false;

	setBlockNoCheck(pos, layer, block);
	return true;
}
