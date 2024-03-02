#pragma once

#include "types.h" // bid_t
#include <string>

struct lua_State;
class Player;

class Script {
public:
	~Script();

	bool init();
	bool loadFromFile(const std::string &filename);

	bool loadDefinition(bid_t block_id);
	void whileIntersecting(Player *player);

private:
	lua_State *m_lua = nullptr;
};
