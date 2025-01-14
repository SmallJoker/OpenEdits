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
	void setMyPlayer(const Player *player) { m_my_player = player; }

	bool invoked_by_server = false; /// server-sent events.

protected:
	int implWorldSetTile(PositionRange range, bid_t block_id, int tile) override;

protected:
	bool isMe() const { return *m_player == m_my_player; }
	const Player *m_my_player = nullptr;

	Client *m_client = nullptr;
};
