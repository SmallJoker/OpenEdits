#pragma once

#include "core/macros.h"
#include "core/playerflags.h"
#include "core/types.h"
#include <map>
#include <set>
#include <string>
#include <vector2d.h>

using namespace irr;

class Packet;
class Script;
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

	void setWorld(RefCnt<World> world);
	RefCnt<World> getWorld() const;

	void setScript(Script *script) { m_script = script; }

	void readPhysics(Packet &pkt);
	void writePhysics(Packet &pkt) const;

	PlayerControls getControls() { return m_controls; }
	// True: outdated controls -> send update to server
	bool setControls(const PlayerControls &ctrl);

	void setPosition(core::vector2df newpos, bool reset_progress);

	PlayerFlags getFlags() const;
	// For networking only!
	void writeFlags(Packet &pkt, playerflags_t mask) const;
	void readFlags(Packet &pkt);

	void step(float dtime);

	const peer_t peer_id;
	std::string name;
	core::vector2df pos;
	core::vector2df vel;
	core::vector2df acc;
	blockpos_t last_pos;
	bool did_jerk = false; //< abrupt position changes. e.g. teleporter

	float dtime_delay = 0; //< RTT compensation upon the next ::step call

	// For keys or killing blocks
	std::set<blockpos_t> *on_touch_blocks = nullptr;

	void setGodMode(bool value);
	bool godmode = false;
	bool controls_enabled = true;

	// Resetable progress
	u8 coins = 0;
	blockpos_t checkpoint;
	//std::map<size_t, BlockParams> params;

	u8 smiley_id = 0;

	static constexpr float GRAVITY_NORMAL = 100.0f;
	static constexpr float CONTROLS_ACCEL = 60.0f;
	static constexpr float JUMP_SPEED = 30.0f;

protected:
	Player(peer_t peer_id) :
		peer_id(peer_id), m_world(nullptr) {}

	inline blockpos_t getCurrentBlockPos()
	{ return blockpos_t(pos.X + 0.5f, pos.Y + 0.5f); }

	void stepInternal(float dtime);
	bool stepCollisions(float dtime);
	void collideWith(float dtime, int x, int y);

	// Currently active world (nullptr if lobby)
	RefCnt<World> m_world;

	// For callbacks
	Script *m_script = nullptr;

	PlayerControls m_controls;
	core::vector2d<s8> m_collision;

	float m_jump_cooldown = 0;
};
