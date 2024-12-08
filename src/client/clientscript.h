#pragma once

#include "core/script/script.h"

class Client;

class ClientScript : public Script {
public:
	ClientScript(BlockManager *bmgr) :
		Script(bmgr, ST_CLIENT) {}

	void initSpecifics() override;
	void closeSpecifics() override;

	void setClient(Client *cli) { m_client = cli; }
	void setMyPlayer(const Player *player) { m_my_player = player; }

	bool invoked_by_server = false; /// server-sent events.

protected:
	int implWorldSetTile(PositionRange range, bid_t block_id, int tile) override;

	static int l_gui_change_hud(lua_State *L);
	static int l_gui_play_sound(lua_State *L);

private:
	bool isMe() const { return m_player == m_my_player; }
	const Player *m_my_player = nullptr;

	Client *m_client = nullptr;
};
