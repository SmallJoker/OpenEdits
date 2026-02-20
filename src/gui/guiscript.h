#pragma once

#include "client/clientscript.h"
#include <array>
#include <map>
#include <memory> // unique_ptr

namespace irr {
	class IrrlichtDevice;
	struct SEvent;

	namespace gui {
		class IGUIElement;
		class IGUIEnvironment;
	}
}

namespace guilayout {
	struct Element;
}

struct BlockProperties;
struct BlockUpdate;
struct HudElement;

using namespace irr;
typedef std::unique_ptr<guilayout::Element> EPtr;

class GuiScript : public ClientScript {
public:
	GuiScript(BlockManager *bmgr, gui::IGUIEnvironment *env);
	virtual ~GuiScript();

	void initSpecifics() override;
	void closeSpecifics() override;

	void linkWithGui(BlockUpdate *bu);
	bool OnEvent(const SEvent &e);

	// Opens a GUI. Close with `Block::ID_INVALD`.
	guilayout::Element *openGUI(bid_t block_id, gui::IGUIElement *parent);
	void closeGUI() { openGUI(Block::ID_INVALID, nullptr); }
	const BlockProperties *getCurrentProps() const { return m_props; }

private:
	void onInput(const char *k, const char *v);
public:
	bool onPlace(blockpos_t pos);
	std::vector<bid_t> hotbar_blocks;
private:
	static int l_gui_set_hud(lua_State *L);
	static int l_gui_remove_hud(lua_State *L);
	static int l_gui_play_sound(lua_State *L);
	static int l_gui_select_block(lua_State *L);
	static int l_gui_set_hotbar(lua_State *L);

	gui::IGUIEnvironment *m_guienv;
	BlockUpdate *m_block_update = nullptr;

	// Until the GUI is closed
	const BlockProperties *m_props = nullptr;
	EPtr m_le_root; //< currently open element

	// -------- HUD Elements
public:
	/// Call when redrawing or on exit
	/// @oaram do_remove `true` removes all known HUD elements
	void refreshHUD(bool do_remove);
	/// Call on each draw step to update the visible HUD elements
	void updateHUD(std::array<s16, 4> area);
	void drawHUDElementsDebug(IrrlichtDevice *device) const;
private:
	std::map<int, HudElement> m_hud_elements;
	u8 m_hud_id_next = 100;
};
