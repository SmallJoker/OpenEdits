#pragma once

#include "blockmanager.h"
#include <string>
#include <irrTypes.h>
#include <vector2d.h>

using namespace irr;

typedef core::vector2d<u16> blockpos_t;

class Player;

struct CollisionData {
	Player &player; // includes World reference
	blockpos_t pos;
	core::vector2di direction;
};

struct Block {
	bid_t id;
	uint8_t param1; // rotation?
};

struct WorldMeta {
	std::string key;
	std::string title;
	std::string owner;
	bool is_public = true;
	u16 online = 0;
	u32 plays = 0;
};

class World {
public:
	World();
	virtual ~World();

	void createDummy(blockpos_t size);

	bool getBlock(blockpos_t pos, Block *block) const;
	virtual bool setBlock(blockpos_t pos, Block block);

	blockpos_t getSize() const { return m_size; }
	WorldMeta &getMeta() { return m_meta; }

protected:
	inline Block &getBlockRefNoCheck(blockpos_t pos, char layer) const
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
};
