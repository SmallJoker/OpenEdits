#pragma once

#include "core/types.h" // bid_t
#include <map>
#include <stdint.h>
#include <string>

class BlockManager;
class Client;
class ClientScript;
class Player;
class World;
struct Block;

struct TileCacheEntry {
	TileCacheEntry() {}
	TileCacheEntry(uint8_t t, std::string &&ov) :
		tile(t), overlay(ov) {}

	uint8_t tile;
	std::string overlay;
};

class TileCacheManager {
public:
	TileCacheManager() {}
	void init(ClientScript *script);

	TileCacheEntry getOrCache(const Player *player, const Block *b);
	void clearCacheFor(World *world, bid_t block_id);
	void clearAll() { m_cache.clear(); }

private:
	ClientScript *m_script = nullptr;
	const BlockManager *m_bmgr = nullptr;

	// TODO: Perform a periodic cleanup

	// Key: 0x00000HHHHHHHHTBB (H = params hash, T = tile, B = block ID)
	std::map<size_t, TileCacheEntry> m_cache;
};
