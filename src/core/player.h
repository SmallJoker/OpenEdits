#pragma once

#include "core/macros.h"
#include "core/types.h"
#include <string>

using namespace irr;

class Packet;
class World;

struct PlayerControls {
	bool operator ==(const PlayerControls &o) const
	{
		return o.dir == dir && o.jump == jump;
	}

	core::vector2df dir;
	bool jump = false;
};

class Player {
public:
	virtual ~Player() {}

	void setWorld(World *world);
	World *getWorld();

	void readPhysics(Packet &pkt);
	void writePhysics(Packet &pkt);

	PlayerControls getControls() { return m_controls; }
	// True: outdated controls
	bool setControls(const PlayerControls &ctrl);

	void step(float dtime);

	const peer_t peer_id;
	std::string name;
	core::vector2df pos;
	core::vector2df vel;
	core::vector2df acc;

	bool is_physical = true;
	bool controls_enabled = true;

	static constexpr float GRAVITY_NORMAL = 40.0f;
	static constexpr float CONTROLS_ACCEL = 20.0f;
	static constexpr float JUMP_SPEED = 20.0f;

protected:
	Player(peer_t peer_id) :
		peer_id(peer_id), m_world(nullptr) {}

	void stepInternal(float dtime);
	bool stepCollisions(float dtime);
	void collideWith(float dtime, int x, int y);

	// Currently active world (nullptr if lobby)
	RefCnt<World> m_world;

	PlayerControls m_controls;
	core::vector2d<s8> m_collision;
};
