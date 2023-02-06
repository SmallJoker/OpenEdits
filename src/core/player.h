#pragma once

#include "core/macros.h"
#include <string>
#include <vector2d.h>

using namespace irr;

class Packet;
class World;

class Player {
public:
	virtual ~Player() {}

	void joinWorld(World *world);
	void leaveWorld();
	inline World *getWorld() { return m_world; }

	void readPhysics(Packet &pkt);
	void writePhysics(Packet &pkt);

	virtual void step(float dtime);

	const peer_t peer_id;
	std::string name;
	core::vector2df pos;
	core::vector2df vel;
	core::vector2df acc;

	bool is_physical = true;

protected:
	Player(peer_t peer_id) :
		peer_id(peer_id) {}

	// Currently active world (nullptr if lobby)
	World *m_world = nullptr;
};
