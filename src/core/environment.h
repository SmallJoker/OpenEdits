#pragma once

#include "core/connection.h"
#include <map>
#include <memory>

class BlockManager;
class Player;
class World;

using map_peer_player_t = std::map<peer_t, std::unique_ptr<Player>>;

// Fancy C++17 helper to generate iterate over all `Player`
// instances in the `m_players` map, with balanced brackets.
#if __cplusplus >= 201703L
	#define FOR_PLAYERS(constness, it_name, map) \
		for (constness auto & it_ ## it_name : map) \
			if (constness Player * it_name = (it_ ## it_name).second.get(); 1)
#else
	// Could be implemented with an extra check, `if (TYPE v = getter())`
	#error
#endif

class Environment : public PacketProcessor {
public:
	Environment(BlockManager *bmgr);
	virtual ~Environment();

	virtual void step(float dtime) = 0;

	virtual std::vector<Player *> getPlayersNoLock(const World *world) const;

	static constexpr uint64_t TIME_RESOLUTION = 100;
	/// Returns the system time in respect to TIME_RESOLUTION
	static uint64_t getTimeNowDIV();

protected:

	Connection *m_con = nullptr;
	BlockManager *m_bmgr;

	std::mutex m_players_lock;
	map_peer_player_t m_players;
};
