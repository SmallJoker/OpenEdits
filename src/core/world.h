#pragma once

#include "blockmanager.h"
#include "core/macros.h"
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

struct BlockUpdate : public Block {
	peer_t peer_id;
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
	~World();

	void createEmpty(blockpos_t size);
	void createDummy(blockpos_t size);

	bool getBlock(blockpos_t pos, Block *block, char layer = 0) const;
	bool setBlock(blockpos_t pos, Block block, char layer = 0);

	blockpos_t getSize() const { return m_size; }
	WorldMeta &getMeta() { return m_meta; }

	std::mutex mutex; // used by Server/Client
	std::map<blockpos_t, BlockUpdate> proc_queue; // for networking

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
