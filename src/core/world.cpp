#include "world.h"
#include "macros.h"

World::World()
{
}

World::~World()
{
	delete[] m_data;
}

bool World::getBlock(blockpos_t pos, Block *block)
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y)
		return false;

	auto props = getBlockProperties(block->id);
	if (!props)
		return false;

	if (block)
		*block = getBlockRefNoCheck(pos, props->type == BlockProperties::BT_BACKGROUND);
	return true;
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

bool World::setBlock(blockpos_t pos, Block block)
{
	if (pos.X >= m_size.X || pos.Y >= m_size.Y)
		return false;

	auto props = getBlockProperties(block.id);
	if (!props)
		return false;

	setBlockNoCheck(pos, props->type == BlockProperties::BT_BACKGROUND, block);
	return true;
}

BlockProperties *World::getBlockProperties(bid_t block_id)
{
	auto it = m_props.find(block_id);
	return it != m_props.end() ? &it->second : nullptr;
}


