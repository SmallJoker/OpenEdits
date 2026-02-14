#pragma once

#include <list>
#include <memory> // unique_ptr
#include <string>

namespace irr::gui {
	class IGUIElement;
	class IGUIEnvironment;
}

namespace guilayout {
	struct Element;
}

struct lua_State;

using namespace irr;
typedef std::unique_ptr<guilayout::Element> EPtr;

class GuiBuilder {
public:
	GuiBuilder(lua_State *L, gui::IGUIEnvironment *env);
	~GuiBuilder();

	/// @param ref Lua table reference
	EPtr show(int ref, gui::IGUIElement *parent);
	static void update_input_value(lua_State *L, int ref, gui::IGUIElement *ie);

private:

	// from Lua stack, possibly recursive.
	static int read_element(lua_State *L);

	int m_ref = -2; // LUA_NOREF

	lua_State *m_lua;
	gui::IGUIEnvironment *m_guienv;
	std::list<EPtr> m_le_stack; //< temporary for layout
	std::list<gui::IGUIElement *> m_ie_stack; //< for relative positions and rough tab order
	std::string m_focus; //< name of the focussed element
};


