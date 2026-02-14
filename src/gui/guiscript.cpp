#include "guiscript.h"
#include "guibuilder.h"
#include "client/client.h"
#include "client/gameevent.h"
#include "core/blockmanager.h"
#include "core/script/script_utils.h"
#include "guilayout/guilayout_irrlicht.h"
// Irrlicht includes
#include <IEventReceiver.h> // SEvent
#include <IGUIElement.h>

using namespace guilayout;
using namespace ScriptUtils;

Logger guiscript_logger("GuiScript", LL_INFO);
static Logger &logger = guiscript_logger;

GuiScript::GuiScript(BlockManager *bmgr, gui::IGUIEnvironment *env) :
	ClientScript(bmgr), m_guienv(env)
{
}

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
	FIELD_SET_FUNC(gui_, set_hotbar);
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
				GuiBuilder::update_input_value(m_lua, m_props->ref_gui_def, e->getElement());
			}
			return true; // continue
		});
		return true; // handled entirely
	}

nomatch:
	return false;
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

guilayout::Element *GuiScript::openGUI(bid_t block_id, gui::IGUIElement *parent)
{
	m_le_root = nullptr;

	m_props = m_bmgr->getProps(block_id);
	if (!m_props || m_props->ref_gui_def < 0)
		return nullptr; // no GUI

	lua_State *L = m_lua;
	int top = lua_gettop(L);

	GuiBuilder builder(L, m_guienv);
	m_le_root = builder.show(m_props->ref_gui_def, parent);

	ASSERT_FORCED(lua_gettop(L) == top, "unbalanced stack!");
	return m_le_root.get();
}

// -------------- Static Lua functions -------------

int GuiScript::l_gui_change_hud(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	int id = -1;
	if (!lua_isnil(L, 1))
		id = luaL_checkinteger(L, 1);

	// id < 0 --> assign new
	// TODO
	int hud_not_implemented_yet = 0;
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

	Script::readBlockParams(L, 2, bu->params);
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

int GuiScript::l_gui_set_hotbar(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	GuiScript *script = static_cast<GuiScript *>(get_script(L));
	auto &list = script->hotbar_blocks;
	list.clear();

	for (lua_pushnil(L); lua_next(L, 1); lua_pop(L, 1)) {
		// key @ -2, value @ -1
		bid_t block_id = lua_tonumber(L, -1); // downcast
		list.emplace_back(block_id);
	}

	if (list.size() > 20) {
		logger(LL_WARN, "Too many hotbar blocks (n=%zu)", list.size());
		list.resize(20);
	}

	return 0;
	MESSY_CPP_EXCEPTIONS_END
}
