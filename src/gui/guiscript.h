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
}

struct BlockParams;
class BlockProperties;

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

	// root element
	EPtr getLayout(bid_t block_id);

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
	const BlockProperties *m_props = nullptr;
	std::list<EPtr> m_elements; // stack of GUI elements
	std::set<std::string> m_fields; // what to listen for
};
