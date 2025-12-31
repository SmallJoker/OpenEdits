#include "tilecache.h"
#include "client.h"
#include "clientscript.h"
#include "core/blockmanager.h"
#include "core/packet.h"
#include "core/player.h"
#include "core/types.h" // Block
#include "core/utils.h" // crc32_z
#include "core/worldmeta.h"

void TileCacheManager::init(ClientScript *script)
{
	m_script = script;
	m_bmgr = m_script->getBlockMgr();
}

TileCacheEntry TileCacheManager::getOrCache(const Player *player, const Block *b)
{
	/*
		Performance measurements of a few non-string overlays
		LuaJIT 2.1-93e8799
		CPU: Intel 7th gen, at 800 MHz (fixed)

		Shortest path, early return:   140 ns
		Look up (incl. crc32 calc):    220 .. 330 ns
		Retrieve new visuals from Lua:  12 ..  64 us (avg approx. 20 us)

		Assumed worst-case, entire world: 100 * 100 * 20 us = 200 ms
	*/
	ASSERT_FORCED(m_script, "Missing init");

	auto world = player->getWorld();

	auto get_params = [&world, b] () {
		BlockParams params;
		world->getParams(world->getBlockPos(b), &params);
		return params;
	};

	auto props = m_bmgr->getProps(b->id);
	if (!props || props->tiles.size() <= 1 || !props->haveGetVisuals())
		return TileCacheEntry(b->tile, "");

	// Retrieve from script

	size_t hash = 0
		| (size_t)(b->id)
		| (size_t)(b->tile) << 16;

	BlockParams params = get_params();
	if (params != BlockParams::Type::None) {
		// Maybe cache this?
		Packet pkt;
		params.write(pkt);
		hash |= crc32_z(0, pkt.data(), pkt.size()) << 24;
	}

	auto it = m_cache.find(hash);
	if (it != m_cache.end())
		return it->second;

	this->cache_miss_counter++,

	// Add to cache
	it = m_cache.emplace(hash, TileCacheEntry()).first;

	TileCacheEntry &tce = it->second;
	tce.tile = b->tile;
	m_script->getVisuals(props, params, &tce);
	return tce;
}

void TileCacheManager::clearCacheFor(World *world, bid_t block_id)
{
	const size_t size_before = m_cache.size();
	for (auto it = m_cache.begin(); it != m_cache.end(); ) {
		if ((it->first & 0xFFFF) == block_id) {
			// delete
			m_cache.erase(it++);
		} else {
			// keep
			++it;
		}
	}

	if (m_cache.size() != size_before) {
		removed_caches_counter += size_before - m_cache.size();
		world->markAllModified();
	}
}
