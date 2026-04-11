#include "serverscript.h"
#include "remoteplayer.h"
#include "server.h"
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "core/chatcommand.h"
#include "core/world.h"
#include "core/worldmeta.h"
#include <string.h> // memset

using namespace ScriptUtils;

static Logger &logger = script_logger;

static int l_nop_false(lua_State *L)
{
	return false;
}

void ServerScript::initSpecifics()
{
	// env space
	lua_State *L = m_lua;

	PlayerRef::doRegister(L);
	pushCurrentPlayerRef();

	lua_getglobal(L, "env");
	{
		field_set_function(L, "is_me", l_nop_false);

		lua_newtable(L);
		field_set_function(L, "register_command", ServerScript::l_server_register_command);
		lua_setfield(L, -2, "server");
	}
	{
		lua_getfield(L, -1, "world");
		field_set_function(L, "get_id", ServerScript::l_world_get_id);
		field_set_function(L, "select", ServerScript::l_world_select);
		field_set_function(L, "set_block", ServerScript::l_world_set_block);
		lua_pop(L, 1); // world
	}
	lua_pop(L, 1); // env
}

Environment *ServerScript::getEnv()
{
	return m_server;
}

void ServerScript::onScriptsLoaded()
{
	Script::onScriptsLoaded();
}

// -------- World / events

int ServerScript::l_world_get_id(lua_State *L)
{
	ServerScript *script = (ServerScript *)get_script(L);
	const World *world = script->m_world;
	if (world) {
		lua_pushstring(L, world->getMeta().id.c_str());
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int ServerScript::l_world_select(lua_State *L)
{
	ServerScript *script = (ServerScript *)get_script(L);
	const char *id = luaL_checkstring(L, 1);

	auto world = script->m_server->getWorldNoLock(id);
	script->setWorld(world.get()); // can be nullptr

	lua_pushboolean(L, !!world);
	return 1;
}

int ServerScript::l_world_set_block(lua_State *L)
{
	ServerScript *script = (ServerScript *)get_script(L);
	World *world = script->m_world;
	if (!world)
		luaL_error(L, "no world");

	BlockUpdate bu(script->m_bmgr);
	bid_t block_id = luaL_checknumber(L, 1);
	bu.pos.X = luaL_checknumber(L, 2) + 0.5f;
	bu.pos.Y = luaL_checknumber(L, 3) + 0.5f;
	if (block_id == (0 | BlockUpdate::BG_FLAG)) {
		bu.setErase(true);
	} else {
		bu.set(block_id);
		Script::readBlockParams(L, 4, bu.params);
	}

	// See also: `Server::pkt_PlaceBlock`
	const Block *block = world->updateBlock(bu);
	if (block)
		world->proc_queue.insert(bu);

	return 0;
}

int ServerScript::implWorldSetTile(PositionRange range, bid_t block_id, int tile)
{
	lua_State *L = m_lua;
	if (!m_world)
		luaL_error(L, "no world");

	// Note: There is no broadcast. Client-side updates must be performed by events.
	bool modified = m_world->setBlockTiles(range, block_id, tile);
	lua_pushboolean(L, modified);
	return 1;
}


// -------- Player API


void ServerScript::implSendTeleport(Player *player, core::vector2df pos) const
{
	m_server->teleportPlayer(player, pos);
}


// -------- Chat commands

void ServerScript::runChatCommand(int ref, Player *player, const std::string &msg)
{
	if (ref < 0)
		return;

	setPlayer(player);

	lua_State *L = m_lua;

	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	const int errorhandler = lua_gettop(L);
	lua_pushcfunction(L, l_run_chatcmd);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	lua_pushstring(L, msg.c_str());

	int status = lua_pcall(L, 2, 0, errorhandler);
	if (status != 0) {
		// Get the origin of the Lua function
		lua_Debug ar;
		memset(&ar, 0, sizeof(ar));
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		lua_getinfo(L, ">S", &ar);

		const char *err = lua_tostring(L, -1);
		logger(LL_ERROR, "%s %s:%d failed: %s", __func__, ar.source, ar.lastlinedefined, err);
	}

	lua_settop(L, 0);
}

// This is tedious but needed to properly catch return value errors
int ServerScript::l_run_chatcmd(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START

	ServerScript *script = (ServerScript *)get_script(L);
	// Get now as it may change during execution!
	Player *player = script->getCurrentPlayer();

	lua_call(L, 1, 2);

	bool success = true;
	std::string reply;
	if (!lua_isnil(L, -2)) {
		luaL_checktype(L, -2, LUA_TBOOLEAN);
		success = lua_toboolean(L, -2);
		reply = luaL_checkstring(L, -1);
	}

	if (reply.empty())
		return 0; // executed without reply
	if (!success)
		reply = "ERR: " + reply;

	script->m_server->systemChatSend(player, reply);

	return 0;
	MESSY_CPP_EXCEPTIONS_END
}

int ServerScript::l_server_register_command(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	ServerScript *script = (ServerScript *)get_script(L);

	const std::string name = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);

	const std::string syntax = check_field_string(L, 2, "syntax");
	const std::string desc   = check_field_string(L, 2, "description");

	// Add a reference AFTER all other checks passed!
	int ref = LUA_NOREF;
	function_ref_from_field(L, -1, "run", ref);
	if (ref < 0)
		luaL_error(L, "missing 'run'");


	ChatCommand &cmd = script->m_server->getChatCommand().add("/" + name);
	cmd.setMain([script, ref] (Player *player, std::string msg) {
		script->runChatCommand(ref, player, msg);
	});
	if (!syntax.empty()) {
		cmd.description
			.append("Syntax: /" + name)
			.append(" " + syntax);
	}
	if (!desc.empty()) {
		if (cmd.description.empty())
			cmd.description.append("/" + name + ": ");
		else
			cmd.description.append("\n");

		cmd.description.append(desc);
	}

	auto it = script->m_chatcmd_refs.find(name);
	if (it != script->m_chatcmd_refs.end()) {
		luaL_unref(L, LUA_REGISTRYINDEX, it->second);
	}
	script->m_chatcmd_refs[name] = ref;

	MESSY_CPP_EXCEPTIONS_END
	return 0;
}
