#pragma once

#include "blockmanager.h"
#include <string>
#include <irrTypes.h>
#include <vector2d.h>

using namespace irr;

typedef core::vector2d<u16> blockpos_t;

struct Block {
	bid_t id;
	uint8_t param1; // rotation?
};

struct WorldMeta {
	std::string key;
	bool is_public = true;
	u32 plays = 0;
};

class World {
public:
	World(BlockManager &blockmgr);
	virtual ~World();

	void createDummy(blockpos_t size);

	bool getBlock(blockpos_t pos, Block *block);
	virtual bool setBlock(blockpos_t pos, Block block);

	WorldMeta &getMeta() { return m_meta; }

protected:
	inline Block &getBlockRefNoCheck(blockpos_t pos, char layer)
	{
		return m_data[(layer * m_size.Y + pos.Y) * m_size.X + pos.X];
	}
	inline void setBlockNoCheck(blockpos_t pos, char layer, Block block)
	{
		m_data[(layer * m_size.Y + pos.Y) * m_size.X + pos.X] = block;
	}

	blockpos_t m_size;
	WorldMeta m_meta;
	Block *m_data = nullptr;

private:
	BlockManager &m_blockmgr;
};
