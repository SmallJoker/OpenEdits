#pragma once

#include "types.h" // bid_t
#include <string>

struct BlockProperties;
struct lua_State;
class BlockManager;
class Player;

class Script {
public:
	Script(BlockManager *bmgr);
	~Script();

	bool init();
	void close();

	/// returns true on success
	bool loadFromFile(const std::string &filename);

	bool loadDefinition(bid_t block_id);

	void setPlayer(Player *player) { m_player = player; }

	/// Setter for `env.test_mode` (unittests)
	void setTestMode(const std::string &value);

	bool haveOnIntersect(const BlockProperties *props) const;
	void onIntersect(const BlockProperties *props);

	struct CollisionInfo {
		const BlockProperties *props = nullptr;
		blockpos_t pos;
		bool is_x = false;
	};
	bool haveOnCollide(const BlockProperties *props) const;
	/// Returns a valid value of BlockProperties::CollisionType
	int onCollide(CollisionInfo ci);

	bool do_load_string_n_table = false;

private:
	static int l_register_pack(lua_State *L);
	static int l_change_block(lua_State *L);

	// Player API
	static int l_player_get_pos(lua_State *L);
	static int l_player_set_pos(lua_State *L);
	static int l_player_get_acc(lua_State *L);
	static int l_player_set_acc(lua_State *L);

	lua_State *m_lua = nullptr;
	BlockManager *m_bmgr = nullptr;
	Player *m_player = nullptr;
};
