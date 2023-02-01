#pragma once

#include "core/connection.h"
#include <map>

class Player;
class World;

typedef uint8_t worldid_t;

class Environment : public PacketProcessor {
public:
	virtual ~Environment() {}

	virtual World *getWorld(Player *who) { return nullptr; }

protected:
	Connection *m_con = nullptr;

	std::map<peer_t, Player *> m_players;
};
