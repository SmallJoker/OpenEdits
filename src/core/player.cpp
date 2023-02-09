#include "player.h"
#include "packet.h"
#include "world.h"
#include <rect.h>

constexpr float DISTANCE_STEP = 0.3f;

inline static float get_sign(float f)
{
	if (f > 0.0001f)
		return 1;
	if (f < -0.0001f)
		return -1;
	return 0;
}

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

	float distance = (((0.5f * acc * dtime) + vel) * dtime).getLength();

	if (distance > DISTANCE_STEP) {
		printf("step(): distance was %f\n", distance);

		float dtime2 = DISTANCE_STEP / (acc * dtime + vel).getLength();
		while (dtime > dtime2) {
			step(dtime2);
			dtime -= dtime2;
		}
	}

	pos += ((0.5f * acc * dtime) + vel) * dtime;
	vel += acc * dtime;
	acc = core::vector2df(0, 0);
	//printf("dtime=%f, y=%f, y.=%f, y..=%f\n", dtime, pos.Y, vel.Y, acc.Y);

	// Apply controls
	acc += m_controls.dir * 10;
	//printf("ctrl x=%g,y=%g\n", m_controls.dir.X, m_controls.dir.Y);

	{
		const float coeff_b = 0.1f; // Stokes
		const float coeff_n = 0.05f; // Newton
		acc.X += ((-coeff_n * vel.X) - coeff_n) * vel.X;
		acc.Y += ((-coeff_n * vel.Y) - coeff_n) * vel.Y;
		const float coeff_f = 50.0f; // Friction
		acc.X += -get_sign(vel.X) * dtime * coeff_f;
		acc.Y += -get_sign(vel.Y) * dtime * coeff_f;
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

	core::vector2di dir;
	dir.X = get_sign(vel.X);
	dir.Y = get_sign(vel.Y);

	auto props = g_blockmanager->getProps(block.id);
	// single block effect
	if (props && props->step) {
		CollisionData c {
			.player = *this,
			.pos = bp,
			.direction = dir
		};

		props->step(dtime, c);
	}

	// Get nearest solid block to collide with
	const int sign_x = get_sign(vel.X);
	const int sign_y = get_sign(vel.Y);

	bool collided = false;

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

			bool ok = m_world->getBlock(blockpos_t(bx, by), &block);
			if (!ok)
				continue;

			auto props = g_blockmanager->getProps(block.id);
			if (!props || props->type != BlockDrawType::Solid)
				continue;

			if (collideWith(bx, by))
				collided = true;
		}
	}

	acc.Y += 5.0f; // DEBUG

	if (!collided) {
		// free falling?
		return;
	}
	//printf("closest: x=%d,y=%d\n", nearest_bp.X, nearest_bp.Y);

	// Do collision handling
	//ok = m_world->getBlock(nearest_bp, &block);
	//props = g_blockmanager->getProps(block.id);
	// maybe run a callback here to check for actual collisions or one-way-gates


	m_collided = collided; // Not a stable value!

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

bool Player::collideWith(int x, int y)
{
	core::rectf player(0, 0, 1, 1);
	core::rectf block(0, 0, 1, 1);
	block += core::vector2df(x - pos.X, y - pos.Y);

	player.clipAgainst(block);
	if (player.getArea() == 0)
		return false;

	//printf("collision x=%d,y=%d\n", x, y);

	if (player.getWidth() > player.getHeight()) {
		vel.Y = 0;
		pos.Y = std::roundf(pos.Y);
	} else {
		vel.X = 0;
		pos.X = std::roundf(pos.X);
	}

	return true;
}

