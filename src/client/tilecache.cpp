#include "tilecache.h"
#include "client.h"
#include "clientscript.h"
#include "core/blockmanager.h"
#include "core/packet.h"
#include "core/player.h"
#include "core/types.h" // Block
#include "core/utils.h" // crc32_z
#include "core/worldmeta.h"

#if 0
	#define DEBUG_LOG(...) printf(__VA_ARGS__)
#else
	#define DEBUG_LOG(...) do {} while (0)
#endif

void TileCacheManager::init(ClientScript *script, RefCnt<World> world)
{
	ASSERT_FORCED(script, "Script missing");
	m_script = script;
	m_bmgr = m_script->getBlockMgr();
	m_world = world;
}

void TileCacheManager::reset()
{
	m_script = nullptr;
	m_bmgr = nullptr;
	m_world.reset();

	clearAll();
}

TileCacheEntry TileCacheManager::getOrCache(const Block *bptr)
{
	/*
		Performance measurements of a few non-string overlays
		LuaJIT 2.1-93e8799
		CPU: Intel 7th gen, at 800 MHz (fixed)

		Shortest path, early return:   110 .. 150 ns
		Look up (incl. crc32 calc):    1.5 ..  2.4 us
		Retrieve new visuals from Lua: 9   .. 10   us

		Performance with 'm_params_hashes':
		Shortest path, early return:    90 .. 150 ns
		Look up (incl. crc32 calc):    0.7 ..  1.5 us
		Retrieve new visuals from Lua: 9   .. 11 us

	*/
	ASSERT_FORCED(m_script, "Missing init");

	const Block b = *bptr;
	auto props = m_bmgr->getProps(b.id);
	if (!props || props->tiles.size() <= 1 || !props->haveGetVisuals())
		return TileCacheEntry(b.tile, "");

	// Check whether we have something cached ...

	BlockParams params;

	paramshash_t params_hash;
	bool was_cached = getParamsHash(bptr, params, &params_hash);
	const size_t hash = 0
		| (size_t)(b.id)
		| (size_t)(b.tile) << 16
		| (size_t)(params_hash) << 24;

	auto it = m_cache.find(hash);
	if (it != m_cache.end())
		return it->second;

	DEBUG_LOG("GET VISUALS id=%d, tile=%d, type=%d, hash=%lX\n",
		b.id, (int)b.tile, (int)params.getType(), hash
	);
	this->cache_miss_counter++;

	if (was_cached) {
		// Needed for 'getVisuals'
		m_world->getParams(m_world->getBlockPos(bptr), &params);
	}

	// Add to cache
	it = m_cache.emplace(hash, TileCacheEntry()).first;

	// Retrieve from script
	TileCacheEntry &tce = it->second;
	tce.tile = b.tile;
	m_script->getVisuals(props, params, &tce);
	return tce;
}

void TileCacheManager::clearCacheFor(bid_t block_id)
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
		DEBUG_LOG("CLEAR CACHE id=%d\n", block_id);
		removed_caches_counter += size_before - m_cache.size();

		m_params_hashes.clear();
		this->params_hash_cache_eff = 0;

		m_world->markAllModified();
	}
}


bool TileCacheManager::getParamsHash(const Block *b, BlockParams &params,
	paramshash_t *hash_out)
{
	if (m_params_hashes.empty()) {
		blockpos_t size = m_world->getSize();
		m_params_hashes.resize(size.X * size.Y);
	}

	const size_t block_i = b - m_world->begin();

	uint32_t params_hash = m_params_hashes[block_i];
	if (params_hash != 0) {
		*hash_out = params_hash;
		this->params_hash_cache_eff++;
		return true;
	}
	this->params_hash_cache_eff--;

	// Add to cache
	m_world->getParams(m_world->getBlockPos(b), &params);
	if (params != BlockParams::Type::None) {
		Packet pkt;
		params.write(pkt);
		params_hash = crc32_z(0, pkt.data(), pkt.size());
	}
	params_hash += !params_hash; // make non-zero
	m_params_hashes[block_i] = params_hash;

	DEBUG_LOG("HASH PARAMS id=%d, tile=%d, hash=%X\n",
		b->id, (int)b->tile, params_hash
	);

	*hash_out = params_hash;
	return false;
}
