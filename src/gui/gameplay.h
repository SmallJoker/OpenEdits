#pragma once

#include "gui.h"
#include <string>
#include <vector3d.h>

namespace irr {
	namespace scene {
		class IBillboardSceneNode;
		class ICameraSceneNode;
		class ISceneNode;
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
	void setCamera(core::vector3df pos);

	bool m_need_mesh_update = false;
	core::vector2df camera_pos;
	core::recti draw_area;

	scene::ISceneNode *m_stage = nullptr;
	scene::ICameraSceneNode *m_camera = nullptr;
};

