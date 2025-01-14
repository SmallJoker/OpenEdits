#include "guiscript.h"
#include "client/client.h"
#include "client/gameevent.h"
#include "client/hudelement.h"
#include "core/blockmanager.h"
#include "core/script/script_utils.h"
#include "guilayout/guilayout_irrlicht.h"
// Irrlicht includes
#include <IGUIEditBox.h>
#include <IGUIStaticText.h>

using namespace guilayout;
using namespace ScriptUtils;
using HET = HudElement::Type;

static Logger logger("GuiScript", LL_DEBUG);

enum GUIElementType {
	ELMT_TABLE   = 1,

	ELMT_TEXT    = 5,
	ELMT_NUMERIC = 6,
};

void GuiScript::initSpecifics()
{
	ClientScript::initSpecifics();
	lua_State *L = m_lua;

#define FIELD_SET_FUNC(prefix, name) \
	field_set_function(L, #name, GuiScript::l_ ## prefix ## name)

	lua_newtable(L);
	FIELD_SET_FUNC(gui_, change_hud);
	FIELD_SET_FUNC(gui_, play_sound);
	// TODO: implement `select_params`
	lua_setglobal(L, "gui");

#undef FIELD_SET_FUNC
}

void GuiScript::closeSpecifics()
{
	ClientScript::closeSpecifics();
}

/// `true` if OK
static bool read_u16_array(lua_State *L, const char *field, u16 *ptr, size_t len)
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

#include "tests/unittest_internal.h"
int GuiScript::gui_read_element(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiScript *script = static_cast<GuiScript *>(get_script(L));

	int type = check_field_int(L, -1, "type");
	logger(LL_DEBUG, "read element T=%d", type);
	std::unique_ptr<Element> e;

	switch ((GUIElementType)type) {
	case ELMT_TABLE:
		{
			Table *t = new Table();
			e.reset(t);

			u16 grid[2];
			if (read_u16_array(L, "grid", grid, 2)) {
				t->setSize(grid[0], grid[1]);
			}

			lua_getfield(L, -1, "fields");
			for (lua_pushnil(L); lua_next(L, 2); lua_pop(L, 1)) {
				// key @ -2, value @ -1
				size_t i = luaL_checkint(L, -2) - 1; // convert to x/y index
				luaL_checktype(L, -1, LUA_TTABLE);
				u16 y = i / grid[0];
				u16 x = i - y * grid[1];

				gui_read_element(L);
				t->set(x, y, script->m_elements.back().release());
				script->m_elements.pop_back();
			}
			lua_pop(L, 1); // fields
		}
		break;
	case ELMT_TEXT:
		{
			const char *text = check_field_string(L, -1, "text");
			core::stringw wtext;
			core::utf8ToWString(wtext, text);
			auto ie = script->m_guienv->addStaticText(wtext.c_str(), core::recti());
			ie->setOverrideColor(0xFFFFFFFF);
			e.reset(new IGUIElementWrapper(ie));
		}
		break;
	case ELMT_NUMERIC:
		{
			const char *name = check_field_string(L, -1, "name");
			// TOOD: retrieve current from "values" table

			auto ie = script->m_guienv->addEditBox(L"", core::recti());
			e.reset(new IGUIElementWrapper(ie));
			ie->setName(name);
		}
		break;
	default:
		logger(LL_WARN, "Unknown element: %d", type);
	}

	read_u16_array(L, "margin", e->margin.data(), e->margin.size());
	read_u16_array(L, "expand", e->expand.data(), e->expand.size());
	read_u16_array(L, "min_size", e->min_size.data(), e->min_size.size());

	script->m_elements.push_back(std::move(e));

	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

EPtr GuiScript::getLayout(bid_t block_id)
{
	auto props = m_bmgr->getProps(block_id);
	if (!props || props->ref_gui_def < 0)
		return nullptr; // no GUI

	lua_State *L = m_lua;

	m_elements.clear();

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	luaL_checktype(L, -1, LUA_TFUNCTION);
	int errorhandler = lua_gettop(L);

	// Function
	lua_pushcfunction(L, gui_read_element);
	// Argument 1
	lua_rawgeti(L, LUA_REGISTRYINDEX, props->ref_gui_def);
	luaL_checktype(L, -1, LUA_TTABLE);

	int status = lua_pcall(L, 1, 0, errorhandler);
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		logger(LL_ERROR, "%s failed: %s", __func__, err);
	}
	lua_settop(L, top);

	MESSY_CPP_EXCEPTIONS_START {
		// read "values"
		// read "pre_place"
	} MESSY_CPP_EXCEPTIONS_END

	lua_pop(L, 1); // gui_def table

	if (m_elements.empty())
		return nullptr;

	return std::move(m_elements.front());
}

int GuiScript::l_gui_change_hud(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	int id = luaL_checkinteger(L, 1);
	int type = HET::T_MAX_INVALID;

	// TODO: get HUD ptr if available

	lua_getfield(L, 2, "type");
	if (!lua_isnil(L, -1)) {
		type = (HET)luaL_checkinteger(L, -1);
	}
	lua_pop(L, 1); // type

	if (type < 0 || type >= HET::T_MAX_INVALID) {
		logger(LL_WARN, "change_hud: unknown type %d", (int)type);
		return 0;
	}

	HudElement e((HET)type);

	switch ((HET)type) {
		case HET::T_TEXT:
			{
				lua_getfield(L, 2, "value");
				*e.params.text = luaL_checkstring(L, -1);
				lua_pop(L, 1);
			}
			break;
		case HET::T_MAX_INVALID:
			// not reachable
			break;
	}

	// TODO: GameEvent to add the HUD
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

int GuiScript::l_gui_play_sound(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiScript *script = static_cast<GuiScript *>(get_script(L));
	const char *sound_name = luaL_checkstring(L, 1);

	if (!script->m_client)
		luaL_error(L, "missing client");

	GameEvent e(GameEvent::C2G_SOUND_PLAY);
	e.text = new std::string(sound_name);
	script->m_client->sendNewEvent(e);

	return 0;
	MESSY_CPP_EXCEPTIONS_END
}
