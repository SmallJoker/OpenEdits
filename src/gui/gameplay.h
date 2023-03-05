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

struct BlockTile;
struct BlockProperties;
class SceneBlockSelector;
class SceneMinimap;
class SceneSmileySelector;

class SceneGameplay : public SceneHandler {
public:
	SceneGameplay();
	~SceneGameplay();

	void OnOpen() override;
	void OnClose() override;

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	// For Minimap
	core::recti getDrawArea() { return m_draw_area; }

private:
	bool handleChatInput(const SEvent &e);
	std::wstring m_previous_chat_message;

	bool getBlockFromPixel(int x, int y, blockpos_t &bp);

	video::ITexture *generateTexture(const std::string &text, u32 color = 0xFFFFFFFF, u32 bgcolor = 0xFF000000);
	std::map<std::string, video::ITexture *> m_cached_textures;

	void drawBlocksInView();
	bool assignBlockTexture(const BlockTile tile, scene::ISceneNode *node);
	bool m_dirty_worldmesh = false;
	core::recti m_drawn_blocks;

	void updatePlayerlist();
	bool m_dirty_playerlist = false;

	void updatePlayerPositions(float dtime);
	float m_nametag_show_timer = 0;

	void setupCamera();
	void setCamera(core::vector3df pos);

	SceneBlockSelector *m_blockselector = nullptr;
	SceneMinimap *m_minimap = nullptr;
	SceneSmileySelector *m_smileyselector = nullptr;

	core::recti m_draw_area; // rendering area

	// Status indicators for mouse inputs
	bool m_may_drag_draw = true;   // permission: free drawing
	BlockUpdate m_drag_draw_block; // drawing mode
	bool m_erase_mode = false;     // removes the pointed block : shift down

	core::vector3df m_camera_pos;  // setter camera position (smoothed)
	scene::ISceneManager *m_world_smgr = nullptr;

	scene::ISceneNode *m_stage = nullptr;
	scene::ICameraSceneNode *m_camera = nullptr;

	gui::IGUIEditBox *m_chathistory = nullptr;
	core::stringw m_chathistory_text;
	bool m_chathistory_text_dirty = false;

	scene::ISceneNode *m_players = nullptr;
};

