#include "guibuilder.h"
#include "core/script/script_utils.h"
#include "guilayout/guilayout_irrlicht.h"
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIStaticText.h>

using namespace guilayout;
using namespace irr;
using namespace ScriptUtils;

enum GUIElementType {
	ELMT_TABLE   = 1,

	ELMT_TEXT    = 5,
	ELMT_INPUT   = 6,
};

extern Logger guiscript_logger;
static Logger &logger = guiscript_logger;


/// `true` if OK
static bool read_s16_array(lua_State *L, const char *field, s16 *ptr, size_t len)
{
	u64 set_i = 0;
	lua_getfield(L, -1, field);
	if (lua_isnil(L, -1))
		goto done;
	luaL_checktype(L, -1, LUA_TTABLE);
	if (len >= 8 * sizeof(set_i))
		goto done; // implementation limit

	for (lua_pushnil(L); lua_next(L, -2); lua_pop(L, 1)) {
		// key @ -2, value @ -1
		size_t i = luaL_checkint(L, -2) - 1;
		if (i < len) {
			ptr[i] = luaL_checkint(L, -1); // downcast
			//logger(LL_DEBUG, "%s [%zu] = %d", field, i, ptr[i]);
			set_i |= 1 << i;
		} else {
			logger(LL_ERROR, "Index %zu of '%s' >= %zu", i, field, len);
		}
	}

done:
	lua_pop(L, 1); // field
	if (set_i == 0)
		return false; // no values provided

	const u64 expected = ((u64)1 << len) - 1;
	if (set_i != expected) {
		logger(LL_WARN, "Missing values for '%s'. want=0x%zX, got=0x%zX", field, expected, set_i);
		return false;
	}
	return true;
}


GuiBuilder::GuiBuilder(lua_State *L, gui::IGUIEnvironment *env) :
	m_lua(L), m_guienv(env)
{
}

GuiBuilder::~GuiBuilder()
{

}


EPtr GuiBuilder::show(int ref, gui::IGUIElement *parent)
{
	m_ref = ref;
	m_le_stack.clear();
	m_ie_stack.clear();
	m_ie_stack.push_back(parent);

	lua_State *L = m_lua;

	lua_pushlightuserdata(L, this);
	lua_rawseti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GUIBUILDER);

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	int errorhandler = lua_gettop(L);

	// Function
	lua_pushcfunction(L, read_element);
	// Argument 1
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

	int status = lua_pcall(L, 1, 0, errorhandler);
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		logger(LL_ERROR, "%s failed: %s", __func__, err);
		m_le_stack.clear();
	}

	lua_settop(L, top);
	m_ie_stack.clear(); // may become invalid later

	if (m_le_stack.empty())
		return nullptr;

	if (m_le_stack.size() != 1) {
		logger(LL_ERROR, "%s: logic error", __func__);
		return nullptr;
	}

	return std::move(m_le_stack.front());
}

void GuiBuilder::update_input_value(lua_State *L, int ref, gui::IGUIElement *ie)
{
	const char *name = ie->getName();
	if (!name[0])
		return;

	logger(LL_INFO, "%s, name=%s", __func__, name);

	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	lua_getfield(L, -1, "values");
	lua_remove(L, -2); // "gui_def"
	lua_getfield(L, -1, name);
	lua_remove(L, -2); // "values"

	const char *text = lua_tostring(L, -1);
	if (!text)
		goto done;

	{
		core::stringw wtext;
		core::utf8ToWString(wtext, text);
		ie->setText(wtext.c_str());
	}

done:
	lua_pop(L, 1);
}

int GuiBuilder::read_element(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiBuilder *self;
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_GUIBUILDER);
		self = (GuiBuilder *)lua_touserdata(L, -1);
		lua_pop(L, 1);
	}

	luaL_checktype(L, -1, LUA_TTABLE);

	if (self->m_le_stack.empty()) {
		// Root table --> meta information
		lua_getfield(L, -1, "focus");
		const char *name = lua_tostring(L, -1);
		if (name)
			self->m_focus = name;
		lua_pop(L, 1);

		lua_getfield(L, -1, "values");
		luaL_checktype(L, -1, LUA_TTABLE);
		lua_pop(L, 1);

		lua_getfield(L, -1, "on_input");
		luaL_checktype(L, -1, LUA_TFUNCTION);
		lua_pop(L, 1);
	}

	int type = check_field_int(L, -1, "type");
	logger(LL_DEBUG, "read element T=%d", type);

	auto &e = self->m_le_stack.emplace_back();
	auto guienv = self->m_guienv;
	auto parent = self->m_ie_stack.back();

	switch ((GUIElementType)type) {
	case ELMT_TABLE:
		{
			Table *t = new Table();
			e.reset(t);

			s16 grid[2] = { 0, 0 };
			if (read_s16_array(L, "grid", grid, 2)
					&& grid[0] >= 1 && grid[1] >= 1) {
				t->setSize(grid[0], grid[1]);
			}

			logger(LL_DEBUG, "+table w=%d, h=%d", grid[0], grid[1]);

			lua_getfield(L, -1, "fields");
			for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1)) {
				// key @ -2, value @ -1
				size_t i = luaL_checkint(L, -2) - 1; // convert to x/y index
				luaL_checktype(L, -1, LUA_TTABLE);
				u16 y = i / grid[0];
				u16 x = i - y * grid[0];

				logger(LL_DEBUG, "set i=%zu (x=%d, y=%d)", i, x, y);
				read_element(L);
				t->get(x, y) = std::move(self->m_le_stack.back());
				self->m_le_stack.pop_back();
			}
			lua_pop(L, 1); // fields
		}
		break;
	case ELMT_TEXT:
		{
			const char *text = check_field_string(L, -1, "text");
			core::stringw wtext;
			core::utf8ToWString(wtext, text);
			auto ie = guienv->addStaticText(wtext.c_str(), core::recti(), false, true, parent);
			e.reset(new IGUIElementWrapper(ie));

			video::SColor color(0xFF000000);
			lua_getfield(L, -1, "color");
			if (!lua_isnil(L, -1))
				color = luaL_checkinteger(L, -1);
			lua_pop(L, 1); // color

			ie->setOverrideColor(color);
		}
		break;
	case ELMT_INPUT:
		{
			const char *name = check_field_string(L, -1, "name");

			auto ie = guienv->addEditBox(L"", core::recti(), true, parent);
			e.reset(new IGUIElementWrapper(ie));
			ie->setName(name);
			ie->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);

			GuiBuilder::update_input_value(L, self->m_ref, ie);
			if (name == self->m_focus)
				guienv->setFocus(ie);
		}
		break;
	default:
		logger(LL_WARN, "Unknown element: %d", type);
	}

	read_s16_array(L, "margin", e->margin.data(), e->margin.size());
	read_s16_array(L, "expand", e->expand.data(), e->expand.size());
	read_s16_array(L, "min_size", e->min_size.data(), e->min_size.size());
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

