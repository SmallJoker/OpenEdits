#pragma once

#include "blockmanager.h"
#include "core/macros.h"
#include "core/types.h"
#include <string>
#include <unordered_set>

class Player;
class Packet;

// Requirement for the hash function
static_assert(sizeof(size_t) >= sizeof(u64));

struct BlockUpdate {
	static constexpr bid_t BG_FLAG { 0x8000 };

	bool operator ==(const BlockUpdate &o) const
	{
		return pos == o.pos && peer_id == o.peer_id && id == o.id;
	}

	blockpos_t pos;

	peer_t peer_id; // specified by server
	// New block ID (BlockUpdate::BG_FLAG for backgrounds)
	bid_t id = Block::ID_INVALID;
};

struct BlockUpdateHash {
	size_t operator ()(const BlockUpdate &v) const
	{
		// 33 bits needs an u64
		return ((u64)((v.id & BlockUpdate::BG_FLAG) > 0) << 32)
			| ((u64)v.pos.Y << 16)
			| ((u64)v.pos.X << 0);
	}
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

	inline bool isValidPosition(int x, int y) const
	{
		return x >= 0 && x < m_size.X
			&& y >= 0 && y < m_size.Y;
	}

	bool getBlock(blockpos_t pos, Block *block) const;
	bool setBlock(blockpos_t pos, const Block block);
	bool updateBlock(const BlockUpdate bu);

	blockpos_t getSize() const { return m_size; }
	WorldMeta &getMeta() { return m_meta; }

	std::mutex mutex; // used by Server/Client
	std::unordered_set<BlockUpdate, BlockUpdateHash> proc_queue; // for networking

protected:
	inline Block &getBlockRefNoCheck(const blockpos_t pos) const
	{
		return m_data[pos.Y * m_size.X + pos.X];
	}

	blockpos_t m_size;
	WorldMeta m_meta;
	Block *m_data = nullptr;
};
