#pragma once

#include "gui/game_commands.h"
#include "gui/gui.h"
#include <list>
#include <string>

namespace irr {
	namespace gui {
		class IGUIEditBox;
	}
	namespace video {
		class ITexture;
	}
}

class SceneBlockSelector;
class SceneMinimap;
class SceneSmileySelector;
class SceneWorldRender;

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

	video::ITexture *generateTexture(const std::string &text, u32 color = 0xFFFFFFFF, u32 bgcolor = 0xFF000000);

private:
	bool initChatHistory();
	bool handleChatInput(const SEvent &e);
	int m_chat_input_index = -1;
	/// New entries added to the front, old removed from the back
	std::list<std::wstring> m_chat_input_history;

	bool getBlockFromPixel(int x, int y, blockpos_t &bp);

	std::map<std::string, video::ITexture *> m_cached_textures; // for generateTexture

	void updatePlayerlist();
	bool m_dirty_playerlist = false;

	SceneBlockSelector *m_blockselector = nullptr;
	SceneMinimap *m_minimap = nullptr;
	SceneSmileySelector *m_smileyselector = nullptr;
	SceneWorldRender *m_world_render = nullptr;

	// Status indicators for mouse inputs
	bool m_may_drag_draw = true;   // permission: free drawing
	BlockUpdate m_drag_draw_block; // drawing mode
	bool m_erase_mode = false;     // removes the pointed block : shift down

	core::recti m_draw_area; // rendering area

	void updateWorldStuff();
	bool m_dirty_world = false;
	int m_total_coins = 0;

	GameCommands m_gamecmd;

	gui::IGUIEditBox *m_chathistory = nullptr;
	core::stringw m_chathistory_text;
	bool m_chathistory_text_dirty = false;
};

