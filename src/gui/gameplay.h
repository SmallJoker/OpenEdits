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
	namespace video {
		class ITexture;
	}
}

class SceneBlockSelector;

class SceneGameplay : public SceneHandler {
public:
	SceneGameplay();
	~SceneGameplay();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

private:
	bool getBlockFromPixel(int x, int y, blockpos_t &bp);
	video::ITexture *generateTexture(const wchar_t *text, u32 color = 0xFFFFFFFF);

	void updateWorld();
	bool assignBlockTexture(const BlockProperties *props, scene::ISceneNode *node);
	bool m_dirty_worldmesh = false;

	void updatePlayerlist();
	bool m_dirty_playerlist = false;

	void updatePlayerPositions(float dtime);
	float m_nametag_show_timer = 0;

	void setupCamera();
	void setCamera(core::vector3df pos);

	SceneBlockSelector *m_blockselector = nullptr;

	core::recti m_draw_area; // rendering area

	// Statis indicators for mouse inputs
	bool m_may_drag_draw = true;   // permission: free drawing
	bid_t m_drag_draw_block = BLOCKID_INVALID; // drawing mode
	bool m_erase_mode = false;     // removes the pointed block : shift down

	bool m_ignore_keys = false;    // ignore key inputs e.g. when typing
	core::vector3df m_camera_pos;  // setter camera position (smoothed)
	scene::ISceneManager *m_world_smgr = nullptr;

	scene::ISceneNode *m_stage = nullptr;
	scene::ICameraSceneNode *m_camera = nullptr;

	gui::IGUIEditBox *m_chathistory = nullptr;
	core::stringw m_chathistory_text;

	scene::ISceneNode *m_players = nullptr;
};

