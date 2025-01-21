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
	FIELD_SET_FUNC(gui_, select_block);
	lua_setglobal(L, "gui");

#undef FIELD_SET_FUNC
}

void GuiScript::closeSpecifics()
{
	ClientScript::closeSpecifics();
}

void GuiScript::linkWithGui(BlockUpdate *bu)
{
	m_block_update = bu;
}

bool GuiScript::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT && e.GUIEvent.EventType == gui::EGET_EDITBOX_CHANGED) {
		auto ie = e.GUIEvent.Caller;
		auto e = IGUIElementWrapper::find_wrapper(m_le_root.get(), ie);

		// find out whether we should listen
		if (!e || !m_props)
			goto nomatch;

		core::stringc ctext;
		wStringToUTF8(ctext, ie->getText());

		// Call "on_input" and retrieve the new value from the "values" table
		onInput(ie->getName(), ctext.c_str());

		m_le_root->doRecursive([this] (Element *e_raw) -> bool {
			if (auto e = dynamic_cast<IGUIElementWrapper *>(e_raw)) {
				updateInputValue(e->getElement());
			}
			return true; // continue
		});
		return true; // handled entirely
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

void GuiScript::onInput(const char *k, const char *v)
{
	if (!m_props)
		return;

	lua_State *L = m_lua;

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);

	logger(LL_INFO, "on_input, id=%d", m_props->id);
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_props->ref_gui_def);
	lua_getfield(L, -1, "on_input"); // function
	lua_getfield(L, -2, "values"); // arg 1
	lua_pushstring(L, k); // arg 2
	lua_pushstring(L, v); // arg 3

	int status = lua_pcall(L, 3, 0, top + 1);
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		logger(LL_ERROR, "%s", err);
	}
	lua_settop(L, top);
}

bool GuiScript::onPlace(blockpos_t pos)
{
	if (!m_block_update)
		return false;
	auto props = m_bmgr->getProps(m_block_update->getId());
	if (!props || props->ref_gui_def < 0)
		return false;

	lua_State *L = m_lua;

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);

	logger(LL_INFO, "on_place, id=%d", props->id);
	lua_rawgeti(L, LUA_REGISTRYINDEX, props->ref_gui_def);
	lua_getfield(L, -1, "on_place"); // function
	lua_getfield(L, -2, "values"); // arg 1
	lua_pushinteger(L, pos.X); // arg 2
	lua_pushinteger(L, pos.Y); // arg 3

	int status = lua_pcall(L, 3, 0, top + 1);
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		logger(LL_ERROR, "%s", err);
	}
	lua_settop(L, top);
	return true;
}

void GuiScript::updateInputValue(gui::IGUIElement *ie)
{
	const char *name = ie->getName();
	if (!name[0])
		return;

	logger(LL_INFO, "%s, name=%s", __func__, name);

	lua_State *L = m_lua;
	lua_rawgeti(L, LUA_REGISTRYINDEX, m_props->ref_gui_def);
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

int GuiScript::gui_read_element(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiScript *script = static_cast<GuiScript *>(get_script(L));
	if (script->m_le_stack.empty()) {
		// Root table --> check whether the format is OK
		luaL_checktype(L, -1, LUA_TTABLE);

		lua_getfield(L, -1, "focus");
		const char *name = lua_tostring(L, -1);
		if (name)
			script->m_focus = name;
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

	auto &e = script->m_le_stack.emplace_back();
	auto guienv = script->m_guienv;
	auto parent = script->m_ie_stack.back();

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
				t->get(x, y) = std::move(script->m_le_stack.back());
				script->m_le_stack.pop_back();
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

			ie->setOverrideColor(0xFF000000); //(0xFFFFFFFF);
		}
		break;
	case ELMT_INPUT:
		{
			const char *name = check_field_string(L, -1, "name");

			auto ie = guienv->addEditBox(L"", core::recti(), true, parent);
			e.reset(new IGUIElementWrapper(ie));
			ie->setName(name);
			ie->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);

			script->updateInputValue(ie);
			if (name == script->m_focus)
				guienv->setFocus(ie);
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

guilayout::Element *GuiScript::openGUI(bid_t block_id, gui::IGUIElement *parent)
{
	m_le_root = nullptr;
	m_le_stack.clear();
	m_ie_stack.clear();

	m_props = m_bmgr->getProps(block_id);
	if (!m_props || m_props->ref_gui_def < 0)
		return nullptr; // no GUI

	m_ie_stack.push_back(parent);

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

	m_le_root = std::move(m_le_stack.front());
	return m_le_root.get();
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

int GuiScript::l_gui_select_block(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiScript *script = static_cast<GuiScript *>(get_script(L));
	bool block_id_provided = false;
	bid_t block_id;
	if (!lua_isnil(L, 1)) {
		block_id = luaL_checkinteger(L, 1);
		block_id_provided = true;
	}

	BlockUpdate bu_fallback(script->m_bmgr);
	BlockUpdate *bu = script->m_block_update;
	if (!bu)
		bu = &bu_fallback;

	if (block_id_provided) {
		if (!bu->set(block_id))
			luaL_error(L, "invalid block_id=%d", block_id);
	}

	script->readBlockParams(2, bu->params);
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}
