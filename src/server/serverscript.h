#pragma once

#include "core/script/script.h"

class Server;
struct BlockUpdate;

class ServerScript : public Script {
public:
	ServerScript(BlockManager *bmgr, Server *server) :
		Script(bmgr, ST_SERVER),
		m_server(server)
	{}

	bool isElevated() const override { return true; }
	Player *getMyPlayer() const override { return nullptr; }

protected:
	void initSpecifics() override;
	Environment *getEnv() override;

public:
	void onScriptsLoaded() override;


	// -------- World / events
protected:
	static int l_world_get_id(lua_State *L);
	static int l_world_select(lua_State *L);

	static int l_world_set_block(lua_State *L);
	int implWorldSetTile(PositionRange range, bid_t block_id, int tile) override;


	// -------- Player API
protected:
	Server *m_server = nullptr;

};

