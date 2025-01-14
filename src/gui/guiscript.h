#pragma once

#include "client/clientscript.h"
#include <list>
#include <memory> // unique_ptr

namespace irr::gui {
	class IGUIEnvironment;
}

namespace guilayout {
	struct Element;
}

typedef std::unique_ptr<guilayout::Element> EPtr;

class GuiScript : public ClientScript {
public:
	GuiScript(BlockManager *bmgr, irr::gui::IGUIEnvironment *env) :
		ClientScript(bmgr), m_guienv(env) {}

	void initSpecifics() override;
	void closeSpecifics() override;

	// root element
	EPtr getLayout(bid_t block_id);

private:
	// from Lua stack, possibly recursive.
	static int gui_read_element(lua_State *L);

	static int l_gui_change_hud(lua_State *L);
	static int l_gui_play_sound(lua_State *L);

	irr::gui::IGUIEnvironment *m_guienv;
	std::list<EPtr> m_elements;
	int m_index_values; // where the "values" table is located
};
