#pragma once

#include <line3d.h>
#include <rect.h>
#include <vector3d.h>

namespace irr {
	namespace scene {
		class ICameraSceneNode;
		class ISceneNode;
		class ISceneManager;
	}
	namespace video {
		class ITexture;
	}
}

using namespace irr;

class Gui;
class SceneGameplay;
struct BlockTile;

class SceneWorldRender {
public:
	SceneWorldRender(SceneGameplay *parent, Gui *gui);
	~SceneWorldRender();

	void draw();
	void step(float dtime);

	core::vector3df *getCameraPos() { return &m_camera_pos; }
	core::line3df getShootLine(core::vector2di mousepos);

	f32 zoom_factor = 1.0f;

	void markDirty() { m_dirty_worldmesh = true; }

private:
	SceneGameplay *m_gameplay = nullptr;
	Gui *m_gui = nullptr;

	void setCamera(core::vector3df pos);

	scene::ISceneNode *m_blocks_node = nullptr;
	scene::ISceneNode *m_players_node = nullptr;
	scene::ICameraSceneNode *m_camera = nullptr;

	core::vector3df m_camera_pos;  // setter camera position (smoothed)
	scene::ISceneManager *m_world_smgr = nullptr;

	void drawBlocksInView();
	bool assignBlockTexture(const BlockTile tile, scene::ISceneNode *node);
	bool m_dirty_worldmesh = false;
	core::recti m_drawn_blocks; // excess area, more drawn than needed

	void updatePlayerPositions(float dtime);
	float m_nametag_show_timer = 0;
};
