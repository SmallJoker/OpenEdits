#pragma once

#include "core/script/script.h"

class ServerScript : public Script {
public:
	ServerScript(BlockManager *bmgr) :
		Script(bmgr) {}

protected:
	int implWorldSetTile(blockpos_t pos, int tile) override;
};

