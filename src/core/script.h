#pragma once

#include "types.h" // bid_t
#include <string>

struct lua_State;
class BlockManager;
class Player;

class Script {
public:
	Script(BlockManager *bmgr);
	~Script();

	bool init();
	bool loadFromFile(const std::string &filename);

	bool loadDefinition(bid_t block_id);

	void setPlayer(Player *player) { m_player = player; }

	struct IntersectionData {
		float dtime;
		bid_t block_id;
		blockpos_t pos;
	};
	void whileIntersecting(IntersectionData &id);

private:

	static int l_register_pack(lua_State *L);
	static int l_register_block(lua_State *L);

	// Player API
	static int l_player_get_pos(lua_State *L);
	static int l_player_set_pos(lua_State *L);


	std::vector<int> m_ref_while_intersecting;
	lua_State *m_lua = nullptr;
	BlockManager *m_bmgr = nullptr;
	Player *m_player = nullptr;
};
