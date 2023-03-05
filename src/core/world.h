#pragma once

#include "core/blockparams.h"
#include "core/macros.h"
#include "core/playerflags.h"
#include "core/types.h"
#include <string>
#include <map>
#include <unordered_set>

class BlockManager;
class Player;
class Packet;

// Requirement for the hash function
static_assert(sizeof(size_t) >= sizeof(u64));

struct BlockUpdate {
	BlockUpdate(const BlockManager *bmgr) : m_mgr(bmgr) {}

	bool set(bid_t block_id);
	void setErase(bool background);

	bool check(bid_t *block_id, bool *is_bg) const;
	inline bool isBackground() const { return (id & BG_FLAG) > 0; }
	inline bid_t getId() const
	{
		return (id != Block::ID_INVALID) ? (id & ~BG_FLAG) : Block::ID_INVALID;
	}

	void read(Packet &pkt);
	void write(Packet &pkt) const;

	bool operator ==(const BlockUpdate &o) const
	{
		// ID check includes FG/BG
		return pos == o.pos && id == o.id;
	}

	blockpos_t pos;
	BlockParams params;
	peer_t peer_id = -1; // specified by server

private:
	// New block ID (BlockUpdate::BG_FLAG for backgrounds)
	bid_t id = Block::ID_INVALID;
	const BlockManager *m_mgr;

	static constexpr bid_t BG_FLAG { 0x8000 };
};

struct BlockUpdateHash {
	size_t operator ()(const BlockUpdate &v) const
	{
		// 33 bits needs an u64
		return ((u64)v.isBackground() << 32)
			| ((u64)v.pos.Y << 16)
			| ((u64)v.pos.X << 0);
	}
};

struct IWorldMeta {
	// For networking
	void readCommon(Packet &pkt);
	void writeCommon(Packet &pkt) const;

	std::string id;
	std::string title;
	std::string owner;
	bool is_public = true;
	u32 plays = 0;
	u16 online = 0;

protected:
	IWorldMeta(const std::string &id) :
		id(id) {}
};

struct LobbyWorld : public IWorldMeta {
	LobbyWorld(const std::string &id) :
		IWorldMeta(id) {}

	blockpos_t size;
};

// Per-world shared pointer to safely clear and set new world data (swap)
struct WorldMeta : public IWorldMeta, public irr::IReferenceCounted {
	WorldMeta(const std::string &id) :
		IWorldMeta(id) {}

	DISABLE_COPY(WorldMeta);

	bool is_persistent = true; // for testing
	std::string edit_code;

	PlayerFlags getPlayerFlags(const std::string &name) const;
	void setPlayerFlags(const std::string &name, const PlayerFlags pf);
	const std::map<std::string, PlayerFlags> &getAllPlayerFlags() const { return player_flags; }
	// For database
	void readPlayerFlags(Packet &pkt);
	void writePlayerFlags(Packet &pkt) const;

	// Activated keys
	struct Key {
		// refill <  0: restart counting only when expired
		// refill >= 0: restart counting
		bool trigger(float refill);
		bool step(float dtime);

		// Client: cooldown until next sending
		// Server: time until disable
		float cooldown = 0;
		bool active = false;
	} keys[3] = {};

	int spawn_index = -1;

private:
	std::map<std::string, PlayerFlags> player_flags;
};

constexpr u16 PROTOCOL_VERSION_FAKE_DISK = UINT16_MAX;

class World : public IReferenceCounted {
public:
	World(World *copy_from);
	World(const BlockManager *bmgr, const std::string &id);
	~World();

	enum class Method : u8 {
		Dummy = 7, // No-op for testing
		Plain = 41, // Bitmap-alike
		//CompressionV1,
		INVALID
	};

	DISABLE_COPY(World);

	void createEmpty(blockpos_t size);
	void createDummy(blockpos_t size);

	void read(Packet &pkt, u16 protocol_version);
	void write(Packet &pkt, Method method, u16 protocol_version) const;

	inline bool isValidPosition(int x, int y) const
	{
		return x >= 0 && x < m_size.X
			&& y >= 0 && y < m_size.Y;
	}

	bool getBlock(blockpos_t pos, Block *block) const;
	bool setBlock(blockpos_t pos, const Block block);
	blockpos_t getBlockPos(const Block *b) const;
	Block *updateBlock(const BlockUpdate bu);
	bool getParams(blockpos_t pos, BlockParams *params) const;

	const BlockManager *getBlockMgr() const { return m_bmgr; }

	// Result is added when callback is nullptr or returns true
	std::vector<blockpos_t> getBlocks(bid_t block_id, std::function<bool(Block &b)> callback) const;
	Block *begin() const { return m_data; };
	const Block *end() const { return &m_data[m_size.X * m_size.Y]; };

	blockpos_t getSize() const { return m_size; }
	const WorldMeta &getMeta() const { return *m_meta.ptr(); }
	WorldMeta &getMeta() { return *m_meta.ptr(); }

	mutable std::mutex mutex; // used by Server/Client
	std::unordered_set<BlockUpdate, BlockUpdateHash> proc_queue; // for networking

protected:
	inline Block &getBlockRefNoCheck(const blockpos_t pos) const
	{
		return m_data[pos.Y * m_size.X + pos.X];
	}

	void readPlain(Packet &pkt, u16 protocol_version);
	void writePlain(Packet &pkt, u16 protocol_version) const;

	blockpos_t m_size;
	const BlockManager *m_bmgr;
	RefCnt<WorldMeta> m_meta;
	Block *m_data = nullptr;
	std::map<blockpos_t, BlockParams> m_params;
};
