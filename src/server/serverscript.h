#pragma once

#include "core/script/script.h"

class ServerScript : public Script {
public:
	ServerScript(BlockManager *bmgr) :
		Script(bmgr) {}

	void onScriptsLoaded() override;

protected:
	int implWorldSetTile(PositionRange range, bid_t block_id, int tile) override;


	// -------- Player API
public:
	void onPlayerJoin(Player *player);
	void onPlayerLeave(Player *player);
private:
	int m_ref_on_player_join = -2; // LUA_NOREF
	int m_ref_on_player_leave = -2; // LUA_NOREF
};

