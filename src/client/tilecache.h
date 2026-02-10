#pragma once

#include "core/types.h" // bid_t, blockpos_t
#include <map>
#include <stdint.h>
#include <string>

class BlockManager;
class Client;
class ClientScript;
class Player;
class World;
struct Block;
struct BlockParams;

struct TileCacheEntry {
	TileCacheEntry() {}
	TileCacheEntry(uint8_t t, std::string &&ov) :
		tile(t), overlay(ov) {}

	uint8_t tile;
	std::string overlay;
};

using paramshash_t = uint32_t;

class TileCacheManager {
public:
	TileCacheManager() {}
	void init(ClientScript *script, RefCnt<World> world);
	void reset();

	TileCacheEntry getOrCache(const Block *b);
	void clearCacheAt(const Block *b);
	void clearCacheFor(bid_t block_id);
	void clearAll() {
		removed_caches_counter += m_cache.size();
		m_cache.clear();

		m_params_hashes.clear();
		params_hash_cache_eff = 0;
	}

	/// How many tiles that had to be re-cached (0 = world not modified)
	size_t cache_miss_counter = 0;
	/// How many cache entries that were removed on purpose
	size_t removed_caches_counter = 0;

	/// Positive = the cache is useful.
	int64_t params_hash_cache_eff = 0;

private:
	/// Intended to skip BlockParams hashing. size=(width * height)
	std::vector<paramshash_t> m_params_hashes;
	bool getParamsHash(const Block *b, BlockParams &params, paramshash_t *hash_out);

	ClientScript *m_script = nullptr;
	const BlockManager *m_bmgr = nullptr;
	RefCnt<World> m_world;

	// TODO: Perform a periodic cleanup

	// Key: 0x00000HHHHHHHHTBB (H = params hash, T = tile, B = block ID)
	std::map<size_t, TileCacheEntry> m_cache;
};
