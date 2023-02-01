#pragma once

#include <map>
#include <irrTypes.h>
#include <vector2d.h>

using namespace irr;

typedef core::vector2d<u16> blockpos_t;
typedef uint16_t bid_t;

struct BlockProperties;

struct Block {
	bid_t id;
	u8 param1; // rotation?
};

class World {
public:
	World();
	virtual ~World();

	void createDummy(blockpos_t size);

	bool getBlock(blockpos_t pos, Block *block);
	virtual bool setBlock(blockpos_t pos, Block block);

	BlockProperties *getBlockProperties(bid_t block_id);

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
	Block *m_data = nullptr;
	std::map<bid_t, BlockProperties> m_props;

private:
};

struct BlockProperties {
	enum {
		BT_BACKGROUND,
		BT_SOLID,
		BT_ACTION,
		BT_DECORATION
	} type;
	u32 color; // minimap

	void onCollide(World &world) {}
	void step(float dtime) {}
};
