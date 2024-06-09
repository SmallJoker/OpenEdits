#pragma once

#include "core/script/script.h"

class ClientScript : public Script {
public:
	ClientScript(BlockManager *bmgr) :
		Script(bmgr) {}

	void setMyPlayer(const Player *player) { m_my_player = player; }

protected:
	int implWorldSetTile(blockpos_t pos, int tile) override;

private:
	bool isMe() const { return m_player == m_my_player; }
	const Player *m_my_player = nullptr;
};
