#pragma once

#include "core/script/script.h"

class Server;

class ServerScript : public Script {
public:
	ServerScript(BlockManager *bmgr, Server *server) :
		Script(bmgr, ST_SERVER),
		m_server(server)
	{}

	void onScriptsLoaded() override;

protected:
	void initSpecifics() override;

	int implWorldSetTile(PositionRange range, bid_t block_id, int tile) override;


	// -------- Player API
public:
	void onPlayerJoin(Player *player);
	void onPlayerLeave(Player *player);
private:
	static int l_get_players_in_world(lua_State *L);

	int m_ref_on_player_join = -2; // LUA_NOREF
	int m_ref_on_player_leave = -2; // LUA_NOREF
	Server *m_server = nullptr;

};

