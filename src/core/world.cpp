#include "world.h"
#include "macros.h"

World::World()
{
	ASSERT_FORCED(g_blockmanager, "BlockManager is required");
}

World::~World()
{
	delete[] m_data;
}

void World::createDummy(blockpos_t size)
{
	if (size.X == 0 || size.Y == 0)
		throw std::length_error("Invalid size");
	if (m_data)
		throw std::runtime_error("Already created");

	m_size = size;
	m_data = new Block[m_size.X * m_size.Y]; // *2
	for (u16 y = 0; y < (u16)m_size.Y; ++y)
	for (u16 x = 0; x < (u16)m_size.X; ++x) {
		getBlockRefNoCheck({x, y}, 0).id = (y > 5) ? 1 : 0;
	}
}

bool World::getBlock(blockpos_t pos, Block *block) const
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y)
		return false;

	auto props = g_blockmanager->getProps(block->id);
	if (!props)
		return false;

	if (block)
		*block = getBlockRefNoCheck(pos, props->type == BlockDrawType::Background);
	return true;
}

bool World::setBlock(blockpos_t pos, Block block)
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y)
		return false;

	auto props = g_blockmanager->getProps(block.id);
	if (!props)
		return false;

	setBlockNoCheck(pos, props->type == BlockDrawType::Background, block);
	return true;
}
