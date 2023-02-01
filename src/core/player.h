#pragma once

#include <string>
#include <vector2d.h>

using namespace irr;

class World;

class Player {
public:
	virtual ~Player() {}

	void init(World *world) { m_world = world; }

	virtual void step(float dtime);

	std::string name;
	core::vector2df pos;
	core::vector2df vel;
	core::vector2df acc;

	bool is_physical = true;

protected:
	Player() = default;

	World *m_world = nullptr;

private:
};
