#pragma once

#include "blockmanager.h"
#include "core/macros.h"
#include "core/types.h"
#include <string>

class Player;

struct BlockUpdate : public Block {
	peer_t peer_id;
};

struct WorldMeta {
	const std::string id;
	std::string edit_code;
	std::string title;
	std::string owner;
	bool is_public = true;
	u16 online = 0;
	u32 plays = 0;
};


class World : public IReferenceCounted {
public:
	World(const std::string &id);
	~World();

	DISABLE_COPY(World);

	void createEmpty(blockpos_t size);
	void createDummy(blockpos_t size);

	bool getBlock(blockpos_t pos, Block *block, char layer = 0) const;
	bool setBlock(blockpos_t pos, Block block, char layer = 0);

	blockpos_t getSize() const { return m_size; }
	WorldMeta &getMeta() { return m_meta; }

	std::mutex mutex; // used by Server/Client
	std::map<blockpos_t, BlockUpdate> proc_queue; // for networking

protected:
	inline Block &getBlockRefNoCheck(const blockpos_t pos, char layer) const
	{
		return m_data[(layer * m_size.Y + pos.Y) * m_size.X + pos.X];
	}
	inline void setBlockNoCheck(const blockpos_t pos, char layer, const Block block)
	{
		m_data[(layer * m_size.Y + pos.Y) * m_size.X + pos.X] = block;
	}

	blockpos_t m_size;
	WorldMeta m_meta;
	Block *m_data = nullptr;
};
