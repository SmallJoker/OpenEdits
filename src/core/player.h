#pragma once

#include "core/macros.h"
#include "core/types.h"
#include <set>
#include <string>
#include <vector2d.h>

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

	// For keys or killing blocks
	std::set<blockpos_t> *triggered_blocks = nullptr;

	bool godmode = false;
	bool controls_enabled = true;

	static constexpr float GRAVITY_NORMAL = 100.0f;
	static constexpr float CONTROLS_ACCEL = 60.0f;
	static constexpr float JUMP_SPEED = 30.0f;

	// Permission flags. Only valid within the current world
	enum Flag : playerflags_t {
		// World access
		FLAG_BANNED    = 0x01,
		FLAG_EDIT      = 0x02,
		FLAG_EDIT_DRAG = 0x02 + 0x04,
		FLAG_CO_OWNER  = 0x10,
		// Physics
		FLAG_GODMODE = 0x0100,
		FLAG_NO_JUMP = 0x0200,
		FLAG_NO_MOVE = 0x0400
	};

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

	float m_jump_cooldown = 0;
};
