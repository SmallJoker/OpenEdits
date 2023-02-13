#pragma once

#include "blockmanager.h"
#include "core/macros.h"
#include "core/types.h"
#include <string>

class Player;
class Packet;

struct BlockUpdate : public Block {
	peer_t peer_id;
};

struct WorldMeta {
	void readCommon(Packet &pkt);
	void writeCommon(Packet &pkt);

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

	inline bool isValidPosition(int x, int y, int z) const
	{
		return x >= 0 && x < m_size.X
			&& y >= 0 && y < m_size.Y
			&& z >= 0 && z < 2;
	}

	bool getBlock(blockpos_t pos, Block *block) const;
	bool setBlock(blockpos_t pos, Block block);

	blockpos_t getSize() const { return m_size; }
	WorldMeta &getMeta() { return m_meta; }

	std::mutex mutex; // used by Server/Client
	std::map<blockpos_t, BlockUpdate> proc_queue; // for networking

protected:
	inline Block &getBlockRefNoCheck(const blockpos_t pos) const
	{
		return m_data[(pos.Z * m_size.Y + pos.Y) * m_size.X + pos.X];
	}
	inline void setBlockNoCheck(const blockpos_t pos, const Block block)
	{
		m_data[(pos.Z * m_size.Y + pos.Y) * m_size.X + pos.X] = block;
	}

	blockpos_t m_size;
	WorldMeta m_meta;
	Block *m_data = nullptr;
};
