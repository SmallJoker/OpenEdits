#pragma once

#include "gui.h"
#include <string>
#include <vector3d.h>

namespace irr {
	namespace gui {
		class IGUIEditBox;
		class IGUIListBox;
	}
	namespace scene {
		class IBillboardSceneNode;
		class ICameraSceneNode;
		class ISceneNode;
		class ISceneManager;
	}
}

class SceneGameplay : public SceneHandler {
public:
	SceneGameplay();
	~SceneGameplay();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

private:
	blockpos_t getBlockFromPixel(int x, int y);

	void updateWorld();
	void updatePlayerlist();
	void updatePlayerPositions();
	void setupCamera();
	void setCamera(core::vector3df pos);

	bool m_need_mesh_update = false;
	bool m_ignore_keys = false;
	core::vector3df m_camera_pos;
	scene::ISceneManager *m_world_smgr = nullptr;

	scene::ISceneNode *m_stage = nullptr;
	scene::ICameraSceneNode *m_camera = nullptr;

	gui::IGUIEditBox *m_chathistory = nullptr;
	core::stringw m_chathistory_text;

	bool m_need_playerlist_update = false;

	scene::ISceneNode *m_players = nullptr;
};

