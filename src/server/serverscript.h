#pragma once

#include "core/script/script.h"

class ServerScript : public Script {
public:
	ServerScript(BlockManager *bmgr) :
		Script(bmgr) {}

protected:
	int implWorldSetTile(PositionRange range, bid_t block_id, int tile) override;
};

