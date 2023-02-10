#include "player.h"
#include "packet.h"
#include "utils.h"
#include "world.h"
#include <rect.h>

constexpr float DISTANCE_STEP = 0.3f; // absolute max is 0.5f


void Player::joinWorld(World *world)
{
	m_world = world;
	m_world->getMeta().online++;

	pos = core::vector2df();
	vel = core::vector2df();
	acc = core::vector2df();
}

void Player::leaveWorld()
{
	m_world->getMeta().online--;
	m_world = nullptr;
}

void Player::readPhysics(Packet &pkt)
{
	pkt.read(pos.X);
	pkt.read(pos.Y);

	pkt.read(vel.X);
	pkt.read(vel.Y);

	pkt.read(acc.X);
	pkt.read(acc.Y);
}

void Player::writePhysics(Packet &pkt)
{
	pkt.write(pos.X);
	pkt.write(pos.Y);

	pkt.write(vel.X);
	pkt.write(vel.Y);

	pkt.write(acc.X);
	pkt.write(acc.Y);
}

bool Player::setControls(const PlayerControls &ctrl)
{
	bool changed = !(ctrl == m_controls);

	m_controls = ctrl;

	return changed;
}


void Player::step(float dtime)
{
	if (!m_world)
		return;

	m_collision = core::vector2d<s8>(0, 0);

	float distance = (((0.5f * acc * dtime) + vel) * dtime).getLength();

	if (distance > DISTANCE_STEP) {
		//printf("step(): distance was %f\n", distance);

		float dtime2 = DISTANCE_STEP / (acc * dtime + vel).getLength();
		while (dtime > dtime2) {
			stepInternal(dtime2);
			dtime -= dtime2;
		}
	}

	stepInternal(dtime);
}

void Player::stepInternal(float dtime)
{
	pos += ((0.5f * acc * dtime) + vel) * dtime;
	vel += acc * dtime;
	acc = core::vector2df(0, 0);
	//printf("dtime=%f, y=%f, y.=%f, y..=%f\n", dtime, pos.Y, vel.Y, acc.Y);

	{
		const float sign_x = get_sign(vel.X);
		const float sign_y = get_sign(vel.Y);

		const float coeff_b = 0.6f; // Stokes
		acc += -coeff_b * vel;

		const float coeff_n = 0.05f; // Newton
		acc.X += coeff_n * (vel.X * vel.X) * -sign_x;
		acc.Y += coeff_n * (vel.Y * vel.Y) * -sign_y;

		const float coeff_f = 1.0f; // Friction
		acc.X += coeff_f * -sign_x;
		acc.Y += coeff_f * -sign_y;
	}

	auto worldsize = m_world->getSize();
	if (pos.X < 0) {
		pos.X = 0;
		vel.X = 0;
	} else if (pos.X > worldsize.X - 1) {
		pos.X = worldsize.X - 1;
		vel.X = 0;
	}
	if (pos.Y < 0) {
		pos.Y = 0;
		vel.Y = 0;
	} else if (pos.Y > worldsize.Y - 1) {
		pos.Y = worldsize.Y - 1;
		vel.Y = 0;
	}

	// Evaluate center position
	blockpos_t bp(pos.X + 0.5f, pos.Y + 0.5f);
	Block block;
	bool ok = m_world->getBlock(bp, &block);
	if (!ok) {
		fprintf(stderr, "step(): this should not be reached!\n");
		return;
	}

	auto props = g_blockmanager->getProps(block.id);
	// single block effect
	if (props && props->step) {
		props->step(dtime, *this, bp);
	} else {
		// default step
		acc.Y += Player::GRAVITY_NORMAL;
	}

	// Get nearest solid block to collide with
	const int sign_x = get_sign(vel.X);
	const int sign_y = get_sign(vel.Y);

	// Run loop at least once
	bool first_y = true;
	for (int y = 0; y != 2 * sign_y || first_y; y += sign_y) {
		first_y = false;
		bool first_x = true;
		for (int x = 0; x != 2 * sign_x || first_x; x += sign_x) {
			first_x = false;
			if (x == 0 && y == 0)
				continue; // current block

			int bx = bp.X + x,
				by = bp.Y + y;

			collideWith(dtime, bx, by);
		}
	}

	// Controls handling
	if (m_controls.jump) {
		if (get_sign(m_collision.X * acc.X) == 1) {
			vel.X += m_collision.X * -Player::JUMP_SPEED;
		} else if (get_sign(m_collision.Y * acc.Y) == 1) {
			vel.Y += m_collision.Y * -Player::JUMP_SPEED;
		}
	}
	// Apply controls
	acc.X += m_controls.dir.X * Player::CONTROLS_ACCEL;
	if (acc.Y == 0)
		acc.Y += m_controls.dir.Y * Player::CONTROLS_ACCEL;
	//printf("ctrl x=%g,y=%g\n", m_controls.dir.X, m_controls.dir.Y);


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

void Player::collideWith(float dtime, int x, int y)
{
	Block b;
	bool ok = m_world->getBlock(blockpos_t(x, y), &b);
	if (!ok)
		return;

	auto props = g_blockmanager->getProps(b.id);
	if (!props || props->type != BlockDrawType::Solid)
		return;

	core::rectf player(0, 0, 1, 1);
	core::rectf block(0, 0, 1, 1);
	block += core::vector2df(x - pos.X, y - pos.Y);

	player.clipAgainst(block);
	if (player.getArea() == 0)
		return;

	//printf("collision x=%d,y=%d\n", x, y);

	core::vector2d<s8> dir;
	if (player.getWidth() > player.getHeight()) {
		dir.Y = get_sign(vel.Y);
		m_collision.Y = dir.Y;
		if (!props->onCollide || props->onCollide(dtime, *this, dir)) {
			vel.Y = 0;
			pos.Y = std::roundf(pos.Y);
		}
	} else {
		dir.X = get_sign(vel.X);
		m_collision.X = dir.X;
		if (!props->onCollide || props->onCollide(dtime, *this, dir)) {
			vel.X = 0;
			pos.X = std::roundf(pos.X);
		}
	}
}

