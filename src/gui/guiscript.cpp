#include "guiscript.h"
#include "client/client.h"
#include "client/gameevent.h"
#include "client/hudelement.h"
#include "core/blockmanager.h"
#include "core/script/script_utils.h"
#include "guilayout/guilayout_irrlicht.h"
// Irrlicht includes
#include <IEventReceiver.h> // SEvent
#include <IGUIEditBox.h>
#include <IGUIStaticText.h>

using namespace guilayout;
using namespace ScriptUtils;
using HET = HudElement::Type;

static Logger logger("GuiScript", LL_DEBUG);

enum GUIElementType {
	ELMT_TABLE   = 1,

	ELMT_TEXT    = 5,
	ELMT_INPUT   = 6,
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
	FIELD_SET_FUNC(gui_, select_params);
	lua_setglobal(L, "gui");

#undef FIELD_SET_FUNC
}

void GuiScript::closeSpecifics()
{
	ClientScript::closeSpecifics();
}

void GuiScript::linkWithGui(BlockParams *bp)
{
	m_block_params = bp;
}

bool GuiScript::OnEvent(const SEvent &e)
{
	// TODO: find elements by name
	if (e.EventType == EET_GUI_EVENT && e.GUIEvent.EventType == gui::EGET_EDITBOX_CHANGED) {
		auto ie = e.GUIEvent.Caller;
		auto name = ie->getName();
		// find out whether we should listen
		auto it = m_fields.find(name);
		if (it == m_fields.end() || !m_props)
			goto nomatch;

		core::stringc ctext;
		wStringToUTF8(ctext, ie->getText());
		// TODO: Does this need a mutex?
		lua_State *L = m_lua;

		// Call "on_input" and retrieve the new value from the "values" table
		int top = lua_gettop(L);
		lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);

		lua_rawgeti(L, LUA_REGISTRYINDEX, m_props->ref_gui_def);
		lua_getfield(L, -1, "on_input"); // function
		lua_getfield(L, -2, "values"); // arg 1
		lua_pushstring(L, ie->getName()); // arg 2
		lua_pushstring(L, ctext.c_str()); // arg 3

		int status = lua_pcall(L, 3, 0, top + 1);
		if (status != 0) {
			const char *err = lua_tostring(L, -1);
			logger(LL_ERROR, "%s", err);
		}
		lua_settop(L, top);

		updateInputValue(ie);
	}

nomatch:
	return false;
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

void GuiScript::updateInputValue(gui::IGUIElement *ie)
{
	logger(LL_INFO, "%s, name=%s", __func__, ie->getName());

	lua_State *L = m_lua;
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_props->ref_gui_def);
	lua_getfield(L, -1, "values");
	lua_remove(L, -2); // "gui_def"
	lua_getfield(L, -1, ie->getName());
	lua_remove(L, -2); // "values"

	const char *text = lua_tostring(L, -1);
	if (text) {
		core::stringw wtext;
		core::utf8ToWString(wtext, text);
		ie->setText(wtext.c_str());
	}
	lua_pop(L, 1);
}

int GuiScript::gui_read_element(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiScript *script = static_cast<GuiScript *>(get_script(L));
	if (script->m_elements.empty()) {
		// Root table --> check whether the format is OK
		luaL_checktype(L, -1, LUA_TTABLE);

		lua_getfield(L, -1, "values");
		luaL_checktype(L, -1, LUA_TTABLE);
		lua_pop(L, 1);

		lua_getfield(L, -1, "on_input");
		luaL_checktype(L, -1, LUA_TFUNCTION);
		lua_pop(L, 1);
	}

	int type = check_field_int(L, -1, "type");
	logger(LL_DEBUG, "read element T=%d", type);

	auto &e = script->m_elements.emplace_back();

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
				t->get(x, y) = std::move(script->m_elements.back());
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
			e.reset(new IGUIElementWrapper(ie));
			ie->setOverrideColor(0xFFFFFFFF);
		}
		break;
	case ELMT_INPUT:
		{
			const char *name = check_field_string(L, -1, "name");

			auto ie = script->m_guienv->addEditBox(L"", core::recti());
			e.reset(new IGUIElementWrapper(ie));
			ie->setName(name);
			script->m_fields.insert(name);

			script->updateInputValue(ie);
		}
		break;
	default:
		logger(LL_WARN, "Unknown element: %d", type);
	}

	read_u16_array(L, "margin", e->margin.data(), e->margin.size());
	read_u16_array(L, "expand", e->expand.data(), e->expand.size());
	read_u16_array(L, "min_size", e->min_size.data(), e->min_size.size());
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

EPtr GuiScript::getLayout(bid_t block_id)
{
	m_elements.clear();
	m_fields.clear();

	m_props = m_bmgr->getProps(block_id);
	if (!m_props || m_props->ref_gui_def < 0)
		return nullptr; // no GUI

	lua_State *L = m_lua;


	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	int errorhandler = lua_gettop(L);

	// Function
	lua_pushcfunction(L, gui_read_element);
	// Argument 1
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_props->ref_gui_def);

	int status = lua_pcall(L, 1, 0, errorhandler);
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		logger(LL_ERROR, "%s failed: %s", __func__, err);
		m_elements.clear();
		m_fields.clear();
	}

	lua_settop(L, top);

	if (m_elements.empty())
		return nullptr;

	return std::move(m_elements.front());
}

// -------------- Static Lua functions -------------

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

int GuiScript::l_gui_select_params(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiScript *script = static_cast<GuiScript *>(get_script(L));
	if (!script->m_props)
		return 0; // invalid use

	BlockParams bp(script->m_props->paramtypes);
	script->readBlockParams(1, bp);
	if (script->m_block_params)
		*script->m_block_params = bp;

	return 0;
	MESSY_CPP_EXCEPTIONS_END
}
