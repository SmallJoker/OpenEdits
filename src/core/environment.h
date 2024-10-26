#pragma once

#include "core/connection.h"
#include <map>

class BlockManager;
class Player;
class World;

class Environment : public PacketProcessor {
public:
	Environment(BlockManager *bmgr) : m_bmgr(bmgr) {}
	virtual ~Environment() {}

	virtual void step(float dtime) = 0;

	virtual std::vector<Player *> getPlayersNoLock(const World *world);

	static constexpr uint64_t TIME_RESOLUTION = 100;
	/// Returns the system time in respect to TIME_RESOLUTION
	static uint64_t getTimeNowDIV();

protected:
	Connection *m_con = nullptr;
	BlockManager *m_bmgr;

	std::mutex m_players_lock;
	std::map<peer_t, Player *> m_players;
};
