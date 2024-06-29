#include "clientscript.h"
#include "core/script/script_utils.h"
#include "core/blockmanager.h"
#include "core/player.h"
#include "core/world.h"
#include "client.h"
#include "hudelement.h"

using namespace ScriptUtils;
using HET = HudElement::Type;

static Logger &logger = script_logger;

void ClientScript::initSpecifics()
{
	lua_State *L = m_lua;

#define FIELD_SET_FUNC(prefix, name) \
	field_set_function(L, #name, ClientScript::l_ ## prefix ## name)

	lua_newtable(L);
	{
		FIELD_SET_FUNC(gui_, change_hud);
		FIELD_SET_FUNC(gui_, play_sound);
	}
	lua_setfield(L, -2, "gui");

#undef FIELD_SET_FUNC
}

void ClientScript::closeSpecifics()
{
}

int ClientScript::implWorldSetTile(PositionRange range, bid_t block_id, int tile)
{
	if (!isMe())
		return 0; // no-op

	lua_State *L = m_lua;
	World *world = m_player->getWorld().get();
	if (!world)
		luaL_error(L, "no world");

	const BlockProperties *props = m_bmgr->getProps(block_id);
	if (props->tile_dependent_physics) {
		// The server must broadcast this change to all players so they
		// cannot get out of sync (prediction errors)
		// TODO: maybe send a Packet to the server as request?
		luaL_error(L, "Block tile change must be initiated by server");
		return 0;
	}
	// else: We may do it locally for smooth gameplay experience

	world->setBlockTiles(range, block_id, tile);
	return 0;
}

int ClientScript::l_gui_change_hud(lua_State *L)
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

int ClientScript::l_gui_play_sound(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	ClientScript *script = static_cast<ClientScript *>(get_script(L));
	const char *sound_name = luaL_checkstring(L, 1);

	if (!script->m_client)
		luaL_error(L, "missing client");

	GameEvent e(GameEvent::C2G_SOUND_PLAY);
	e.text = new std::string(sound_name);
	script->m_client->sendNewEvent(e);

	return 0;
	MESSY_CPP_EXCEPTIONS_END
}
