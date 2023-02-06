#pragma once

#include "core/connection.h"
#include <map>

class Player;
class World;

class Environment : public PacketProcessor {
public:
	virtual ~Environment() {}

	virtual void step(float dtime) = 0;

protected:
	Connection *m_con = nullptr;

	std::mutex m_players_lock;
	std::map<peer_t, Player *> m_players;
};
