#pragma once

#include "gui.h"
#include <string>

class SceneGameplay : public SceneHandler {
public:
	SceneGameplay();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(const GameEvent &e) override;

private:
	void drawWorld();

	core::vector2df camera_pos;
	core::recti draw_area;
};

