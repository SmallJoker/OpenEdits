#pragma once

#include "client/clientscript.h"
#include <list>
#include <memory> // unique_ptr
#include <set>

namespace irr {
	struct SEvent;

	namespace gui {
		class IGUIElement;
		class IGUIEnvironment;
	}
}

namespace guilayout {
	struct Element;
	struct IGUIElementWrapper;
}

struct BlockParams;
struct BlockProperties;

using namespace irr;
typedef std::unique_ptr<guilayout::Element> EPtr;

class GuiScript : public ClientScript {
public:
	GuiScript(BlockManager *bmgr, gui::IGUIEnvironment *env) :
		ClientScript(bmgr), m_guienv(env) {}

	void initSpecifics() override;
	void closeSpecifics() override;

	void linkWithGui(BlockParams *bp);
	bool OnEvent(const SEvent &e);

	// Opens a GUI. Close with `Block::ID_INVALD`.
	guilayout::Element *openGUI(bid_t block_id, gui::IGUIElement *parent);
	void closeGUI() { openGUI(Block::ID_INVALID, nullptr); }
	const BlockProperties *getCurrentProps() const { return m_props; }

private:
	void updateInputValue(gui::IGUIElement *ie);
	// from Lua stack, possibly recursive.
	static int gui_read_element(lua_State *L);

	static int l_gui_change_hud(lua_State *L);
	static int l_gui_play_sound(lua_State *L);
	static int l_gui_select_params(lua_State *L);

	gui::IGUIEnvironment *m_guienv;
	BlockParams *m_block_params = nullptr;

	// Temporary
	std::list<EPtr> m_le_stack; //< temporary for layout
	std::list<gui::IGUIElement *> m_ie_stack; //< for relative positions and rough tab order
	std::string m_focus; //< name of the focussed element

	// Until the GUI is closed
	const BlockProperties *m_props = nullptr;
	EPtr m_le_root = nullptr; //< currently open element
};
