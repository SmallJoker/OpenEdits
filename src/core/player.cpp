#include "player.h"
#include "packet.h"
#include "world.h"

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

	auto worldsize = m_world->getSize();
	if (pos.X < 0) {
		pos.X = 0;
		vel.X = 0;
	} else if (pos.X - 1 > worldsize.X) {
		pos.X = worldsize.X - 1;
		vel.X = 0;
	}
	if (pos.Y < 0) {
		pos.Y = 0;
		vel.Y = 0;
	} else if (pos.Y - 1 > worldsize.Y) {
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
	float nearest_time = 999;
	blockpos_t nearest_bp = bp;
	for (int y = -1; y < 1; ++y)
	for (int x = -1; x < 1; ++x) {
		if (x == 0 && y == 0)
			continue; // already handled?

		blockpos_t bp2 = bp;
		bp2.X += x; bp2.Y += y; // ignore over/underflow :)

		bool ok = m_world->getBlock(bp2, &block);
		if (!ok)
			continue;

		auto props = g_blockmanager->getProps(block.id);
		if (!props || props->type != BlockDrawType::Solid)
			continue;

		// Calculate time needed to reach his block
		auto time = (core::vector2df(bp2.X, bp2.Y) - pos) / vel;
		if (time.X < nearest_time || time.Y < nearest_time) {
			nearest_bp = bp;
			nearest_time = std::min(time.X, time.Y);
		}
	}

	if (nearest_bp == bp) {
		// free falling?
		return;
	}

	// Do collision handling
	ok = m_world->getBlock(nearest_bp, &block);
	props = g_blockmanager->getProps(block.id);
	// maybe run a callback here to check for actual collisions or one-way-gates

	bool collided = false;
	if (vel.X > 0) {
		if (pos.X > nearest_bp.X - 1) {
			vel.X = 0;
			pos.X = nearest_bp.X - 1;
			collided = true;
		}
	} else if (vel.X < 0) {
		if (pos.X <= nearest_bp.X) {
			vel.X = 0;
			pos.X = nearest_bp.X;
			collided = true;
		}
	}
	if (vel.Y > 0) {
		if (pos.Y > nearest_bp.Y - 1) {
			vel.Y = 0;
			pos.Y = nearest_bp.Y - 1;
			collided = true;
		}
	} else if (vel.Y < 0) {
		if (pos.Y <= nearest_bp.Y) {
			vel.Y = 0;
			pos.Y = nearest_bp.Y;
			collided = true;
		}
	}

	// snap to grid
	if (acc.X * vel.X < 1) {
		// slowing down
		if (std::abs(vel.X) < 0.1f && std::abs(pos.X - bp.X) < 0.1f) {
			pos.X = (int)pos.X;
			vel.X = 0;
		}
	}
	if (acc.Y * vel.Y < 1) {
		// slowing down
		if (std::abs(vel.Y) < 0.1f && std::abs(pos.Y - bp.Y) < 0.1f) {
			pos.Y = (int)pos.Y;
			vel.Y = 0;
		}
	}
}

