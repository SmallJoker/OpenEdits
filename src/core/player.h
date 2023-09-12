#pragma once

#include "core/macros.h"
#include "core/playerflags.h"
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
	virtual ~Player();

	void setWorld(World *world);
	World *getWorld();

	void readPhysics(Packet &pkt);
	void writePhysics(Packet &pkt);

	PlayerControls getControls() { return m_controls; }
	// True: outdated controls -> send update to server
	bool setControls(const PlayerControls &ctrl);

	void setPosition(core::vector2df newpos, bool reset_progress);
	PlayerFlags getFlags() const;

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

	// Resetable progress
	u8 coins = 0;
	blockpos_t checkpoint;

	u8 smiley_id = 0;

	static constexpr float GRAVITY_NORMAL = 100.0f;
	static constexpr float CONTROLS_ACCEL = 60.0f;
	static constexpr float JUMP_SPEED = 30.0f;

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
	blockpos_t m_last_pos;
};
