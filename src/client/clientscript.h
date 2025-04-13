#pragma once

#include "core/script/script.h"

class Client;

class ClientScript : public Script {
public:
	ClientScript(BlockManager *bmgr) :
		Script(bmgr, ST_CLIENT) {}

	virtual void initSpecifics() override;
	virtual void closeSpecifics() override;

	void setClient(Client *cli) { m_client = cli; }
	void setMyPlayer(Player *player) { m_my_player = player; }

	bool isElevated() const override { return invoked_by_server; }
	Player *getMyPlayer() const override { return m_my_player; }

	bool invoked_by_server = false; /// server-sent events.

protected:

	static int l_is_me(lua_State *L);
	static int l_world_update_tiles(lua_State *L);

	int implWorldSetTile(PositionRange range, bid_t block_id, int tile) override;

	// -------- Callbacks
public:
	// Returns the tile number (for now)
	void getVisuals(const BlockProperties *props, uint8_t *tile, const BlockParams &params);

protected:
	Player *m_my_player = nullptr;

	Client *m_client = nullptr;
};
