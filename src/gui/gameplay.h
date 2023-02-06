#pragma once

#include "gui.h"
#include <string>

namespace irr {
	namespace scene {
		class IBillboardSceneNode;
		class ICameraSceneNode;
	}
}

class SceneGameplay : public SceneHandler {
public:
	SceneGameplay();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

private:
	void drawWorld();

	bool m_need_mesh_update = false;
	core::vector2df camera_pos;
	core::recti draw_area;

	scene::IBillboardSceneNode *m_bb = nullptr;
	scene::ICameraSceneNode *m_camera = nullptr;
};

