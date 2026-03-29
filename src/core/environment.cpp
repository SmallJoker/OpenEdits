#include "environment.h"
#include "player.h"
#include "smileymanager.h"
#include <chrono>

Environment::Environment(BlockManager *bmgr) : m_bmgr(bmgr)
{
	m_smileymgr = new SmileyManager();
}

Environment::~Environment()
{
	if (m_smileymgr)
		delete m_smileymgr;
}

std::vector<Player *> Environment::getPlayersNoLock(const World *world) const
{
	std::vector<Player *> ret;
	FOR_PLAYERS(, player, m_players) {
		if (player->getWorld().get() == world)
			ret.push_back(player);
	}
	return ret;
}

uint64_t Environment::getTimeNowDIV()
{
	// This is a monstrosity
	namespace sc = std::chrono;
	auto since_epoch = sc::system_clock::now().time_since_epoch();
	auto elapsed = sc::duration_cast<sc::duration<uint64_t, std::ratio<1, TIME_RESOLUTION>>>(since_epoch);

	return elapsed.count();
}
