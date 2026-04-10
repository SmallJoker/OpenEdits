#pragma once

#include "core/macros.h" // peer_t
#include "core/playerflags.h"
#include "core/script/scriptevent_fwd.h"
#include "core/types.h"
#include <set>
#include <string>

using namespace irr;

class Packet;
class Script;
class ScriptEventManager;
class World;

struct PlayerPhysics {
	float controls_accel = 75.0f;
	float jump_speed = 30.0f;

	void setModified()
	{ resend_counter = 3; }

	/// To call after sending to all peers
	void sent()
	{
		if (resend_counter)
			resend_counter--;
	}

	/// Movement data is sent unreliably, thus send 3 times
	u8 resend_counter = 3;
};

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

	void setScript(Script *script);

	void readPhysics(Packet &pkt);
	/// `send_all`: To be used in Join packets
	void writePhysics(Packet &pkt, bool send_all = false) const;

	PlayerPhysics &getPhysicsRef() { return m_physics; }

	PlayerControls getControls() const { return m_controls; }
	// True: outdated controls -> send update to server
	bool setControls(const PlayerControls &ctrl);

	void setPosition(core::vector2df newpos);

	PlayerFlags getFlags() const;
	// For networking only!
	void writeFlags(Packet &pkt, playerflags_t mask) const;
	void readFlags(Packet &pkt);

	void step(float dtime);

	u32 getNextPRNum();

	const peer_t peer_id;
	std::string name;
	float dtime_delay = 0; //< RTT compensation upon the next ::step call
	core::vector2df pos;
	core::vector2df vel;
	core::vector2df acc;
	blockpos_t last_pos; //< from the last full step
	bool did_jerk = false; //< abrupt position changes. e.g. teleporter
	bool inside_player_step = false; //< gatekeeping for set vel/acc/pos

	inline blockpos_t getCurrentBlockPos() const
	{ return blockpos_t(pos.X + 0.5f, pos.Y + 0.5f); }

	/// Returns `nullptr` when not playing in a world.
	Script *getScript() const { return m_script; }
	ScriptEventManager *getSEMgr() const;

	std::unique_ptr<ScriptEventMap> script_events_to_send;

	void setGodMode(bool value);
	bool godmode = false;

	u8 smiley_id = 0;

	static constexpr float GRAVITY_DEFAULT = 100.0f;

protected:
	Player(peer_t peer_id);

	void stepInternal(float dtime);
	bool stepCollisions(float dtime);
	/// Performs a block collision
	void collideWith(float dtime, int x, int y);

	// Currently active world (nullptr if lobby)
	RefCnt<World> m_world;

	// For callbacks
	Script *m_script = nullptr; //< nullptr when not in a world
	// Note: This should perform better than a boolean (but needs more RAM)
	Script *m_script_backup = nullptr;

	PlayerControls m_controls;
	PlayerPhysics m_physics;
	core::vector2d<s8> m_collision;

	u32 m_prng_state;
	float m_jump_cooldown = 0;
};
