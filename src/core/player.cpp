#include "player.h"
#include "blockmanager.h"
#include "packet.h"
#include "utils.h"
#include "world.h"
#include <rect.h>

constexpr float DISTANCE_STEP = 0.4f; // absolute max is 0.5f
constexpr float VELOCITY_MAX = 200.0f;

Player::~Player()
{
}


void Player::setWorld(World *world)
{
	if (m_world.ptr())
		m_world->getMeta().online--;

	m_world = world;

	if (m_world.ptr())
		m_world->getMeta().online++;

	setPosition({0, 0}, true);
}

World *Player::getWorld()
{
	return m_world.ptr();
}

void Player::readPhysics(Packet &pkt)
{
	pkt.read(pos.X);
	pkt.read(pos.Y);

	pkt.read(vel.X);
	pkt.read(vel.Y);

	pkt.read(acc.X);
	pkt.read(acc.Y);

	pkt.read<u8>(coins); // type safety!

	pkt.read<u8>(); // flags

	m_controls.jump = pkt.read<u8>();
	pkt.read(m_controls.dir.X);
	pkt.read(m_controls.dir.Y);
}

void Player::writePhysics(Packet &pkt)
{
	pkt.write(pos.X);
	pkt.write(pos.Y);

	pkt.write(vel.X);
	pkt.write(vel.Y);

	pkt.write(acc.X);
	pkt.write(acc.Y);

	pkt.write<u8>(coins);

	u8 flags = 0;
	pkt.write(flags);

	pkt.write<u8>(m_controls.jump);
	pkt.write(m_controls.dir.X);
	pkt.write(m_controls.dir.Y);
}

bool Player::setControls(const PlayerControls &ctrl)
{
	bool changed = !(ctrl == m_controls);

	m_controls = ctrl;

	return changed;
}

void Player::setPosition(core::vector2df newpos, bool reset_progress)
{
	if (reset_progress)
		coins = 0;

	pos = newpos;
	vel = core::vector2df();
	acc = core::vector2df();

	m_last_pos = blockpos_t(-1, -1);
}

PlayerFlags Player::getFlags() const
{
	if (!m_world)
		return PlayerFlags(0);
	return m_world->getMeta().getPlayerFlags(name);
}


void Player::step(float dtime)
{
	if (!m_world)
		return;

	m_collision = core::vector2d<s8>(0, 0);
	if (m_jump_cooldown > 0)
		m_jump_cooldown -= dtime;

	//printf("dtime: %g, v=%g, a=%g\n", dtime, vel.getLength(), acc.getLength());

	// Maximal travel distance per iteration
	while (true) {
		float dtime2 = dtime;

		float v = vel.getLength();
		float a = acc.getLength();
		if (a > 0.1f) {
			// 0 = (0.5 * a) * t² + (v) * t - max_d
			// t = (-v ± sqrt(v² + 2*a*max_d)) / a
			float sq = sqrt(v * v + 2 * a * DISTANCE_STEP);
			float t1 = (-v - sq) / a;
			float t2 = (-v + sq) / a;
			if (t1 > 0)
				dtime2 = std::min(dtime2, t1);
			if (t2 > 0)
				dtime2 = std::min(dtime2, t2);
		} else if (v > 0.01f) {
			dtime2 = DISTANCE_STEP / v;
		}

		if (dtime2 * 35.0f < dtime) {
			// The ratio 1/30 is acceptable on 60 FPS, use a bit more just to
			// be sure there are no server-side/unittest issues
			printf("Player:step() takes too long. Approx. %i iterations. STOP!\n", (int)(dtime / dtime2));
			dtime = dtime2;
			break;
		}

		if (dtime2 >= dtime)
			break;

		stepInternal(dtime2);
		dtime -= dtime2;
	}

	stepInternal(dtime);
}

void Player::stepInternal(float dtime)
{
	/*
		Physics issues
		- Sliding into a 1 block gap does not work
			-> Snap feature?
	*/

	pos += ((0.5f * acc * dtime) + vel) * dtime;
	vel += acc * dtime;
	acc = core::vector2df(0, 0);
	//printf("dtime=%f, y=%f, y.=%f, y..=%f\n", dtime, pos.Y, vel.Y, acc.Y);

	{
		// Don't make it get any worse
		if (std::fabs(vel.X) > VELOCITY_MAX)
			vel.X = get_sign(vel.X) * VELOCITY_MAX;

		if (std::fabs(vel.Y) > VELOCITY_MAX)
			vel.Y = get_sign(vel.Y) * VELOCITY_MAX;
	}

	auto worldsize = m_world->getSize();
	if (pos.X < 0) {
		pos.X = 0;
		vel.X = 0;
		m_collision.X = -1;
	} else if (pos.X > worldsize.X - 1) {
		pos.X = worldsize.X - 1;
		vel.X = 0;
		m_collision.X = 1;
	}
	if (pos.Y < 0) {
		pos.Y = 0;
		vel.Y = 0;
		m_collision.Y = -1;
	} else if (pos.Y > worldsize.Y - 1) {
		pos.Y = worldsize.Y - 1;
		vel.Y = 0;
		m_collision.Y = 1;
	}

	// Evaluate center position
	blockpos_t bp(pos.X + 0.5f, pos.Y + 0.5f);
	const BlockProperties *props;
	{
		Block block;
		m_world->getBlock(bp, &block);

		props = g_blockmanager->getProps(block.id);
	}

	if (triggered_blocks && bp != m_last_pos) {
		if (props && props->trigger_on_touch) {
			if (bp != m_last_pos) {
				triggered_blocks->emplace(bp);
			}
		}
		m_last_pos = bp;
	}

	if (!godmode) {
		if (!stepCollisions(dtime))
			return;
	}

	// Controls handling
	if (m_controls.jump && m_jump_cooldown <= 0) {
		if (get_sign(m_collision.X * acc.X) == 1 && std::fabs(vel.X) < 3.0f) {
			vel.X += m_collision.X * -Player::JUMP_SPEED;
			m_jump_cooldown = 0.3f;
		} else if (get_sign(m_collision.Y * acc.Y) == 1 && std::fabs(vel.Y) < 3.0f) {
			vel.Y += m_collision.Y * -Player::JUMP_SPEED;
			m_jump_cooldown = 0.3f;
		}
	}

	// Apply controls
	if (controls_enabled) {
		if (acc.X == 0)
			acc.X += m_controls.dir.X * Player::CONTROLS_ACCEL;
		if (acc.Y == 0)
			acc.Y += m_controls.dir.Y * Player::CONTROLS_ACCEL;
		//printf("ctrl x=%g,y=%g\n", m_controls.dir.X, m_controls.dir.Y);
	}

	{
		// Stokes friction to stop movement after releasing keys
		const float viscosity = props ? props->viscosity : 1.0f;
		const float coeff_s = godmode ? 1.5f : 4.0f * viscosity; // Stokes
		if (std::fabs(acc.X) < 0.01f && !m_controls.dir.X)
			acc.X += -coeff_s * vel.X;
		if (std::fabs(acc.Y) < 0.01f && !m_controls.dir.Y)
			acc.Y += -coeff_s * vel.Y;

		const float sign_x = get_sign(vel.X);
		const float sign_y = get_sign(vel.Y);

		const float coeff_n = 0.04f; // Newton
		acc.X += coeff_n * (vel.X * vel.X) * -sign_x;
		acc.Y += coeff_n * (vel.Y * vel.Y) * -sign_y;

	}

	// snap to grid
	if (acc.X * vel.X < 0.1f) {
		// slowing down
		if (std::abs(vel.X) < 0.1f && std::abs(pos.X - bp.X) < 0.1f) {
			pos.X = std::roundf(pos.X);
			vel.X = 0;
		}
	}
	if (acc.Y * vel.Y < 0.1f) {
		// slowing down
		if (std::abs(vel.Y) < 0.1f && std::abs(pos.Y - bp.Y) < 0.1f) {
			pos.Y = std::roundf(pos.Y);
			vel.Y = 0;
		}
	}
}

bool Player::stepCollisions(float dtime)
{
	blockpos_t bp(pos.X + 0.5f, pos.Y + 0.5f);
	Block block;
	bool ok = m_world->getBlock(bp, &block);
	if (!ok) {
		fprintf(stderr, "step(): this should not be reached!\n");
		return false;
	}

	auto props = g_blockmanager->getProps(block.id);
	// single block effect
	if (props && props->step) {
		props->step(*this, bp);
	} else {
		// default step
		acc.Y += Player::GRAVITY_NORMAL;
	}

	// Collide with direct neighbours, outside afterwards.
	const core::vector2di SCAN_DIR[8] = {
		{  0,  1 },
		{  0, -1 },
		{  1,  0 },
		{ -1,  0 },

		{  1,  1 },
		{  1, -1 },
		{ -1,  1 },
		{ -1, -1 },
	};

	for (const auto dir : SCAN_DIR) {
		int bx = bp.X + dir.X,
			by = bp.Y + dir.Y;

		collideWith(dtime, bx, by);
	}
	return true;
}

void Player::collideWith(float dtime, int x, int y)
{
	Block b;
	blockpos_t bp(x, y);
	bool ok = m_world->getBlock(bp, &b);
	if (!ok)
		return;

	auto props = g_blockmanager->getProps(b.id);
	if (!props || props->getTile(b).type != BlockDrawType::Solid)
		return;

	core::rectf player(0, 0, 1, 1);
	core::rectf block(0, 0, 1, 1);
	block += core::vector2df(x - pos.X, y - pos.Y);

	player.clipAgainst(block);
	if (player.getArea() < 0.001f)
		return;

	//printf("collision x=%d,y=%d\n", x, y);

	const bool is_x = player.getWidth() < player.getHeight();

	using CT = BlockProperties::CollisionType;
	CT type = CT::Position;
	if (props->onCollide)
		type = props->onCollide(*this, bp, is_x);

	switch (type) {
		case CT::Position:
			if (is_x) {
				pos.X = std::roundf(pos.X);
				int sign = get_sign(vel.X);
				if (sign)
					m_collision.X = sign;
			} else {
				pos.Y = std::roundf(pos.Y);
				int sign = get_sign(vel.Y);
				if (sign)
					m_collision.Y = sign;
			}

			// fall through
		case CT::Velocity:
			if (is_x)
				vel.X = 0;
			else
				vel.Y = 0;

			// fall through
		case CT::None:
			if (triggered_blocks && props->trigger_on_touch)
				triggered_blocks->emplace(bp);

			break;
	}

}

