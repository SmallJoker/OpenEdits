#pragma once

#include "core/types.h" // bid_t
#include <string>

struct BlockProperties;
struct lua_State;
class BlockManager;
class MediaManager;
class Player;

class Script {
public:
	Script(BlockManager *bmgr);
	~Script();

	bool init();
	void close();

	void setMediaMgr(MediaManager *media) { m_media = media; }
	/// Safe file loader
	bool loadFromAsset(const std::string &asset_name);

	// -------- For unittests
public:
	/// Setter for `env.test_mode` (unittests)
	void setTestMode(const std::string &value);
	static int popErrorCount();

	// UNSAFE. Accepts any path.
	/// returns true on success
	bool loadFromFile(const std::string &filename);

	bool do_load_string_n_table = false;

	// -------- Registration
private:
	/// Includes another script file (asset from cache or disk)
	static int l_include(lua_State *L);
	static int l_register_pack(lua_State *L);
	static int l_change_block(lua_State *L);


	// -------- Callbacks
public:
	bool haveOnIntersect(const BlockProperties *props) const;
	void onIntersect(const BlockProperties *props);

	bool haveOnIntersectOnce(const BlockProperties *props) const;
	void onIntersectOnce(const BlockProperties *props);

	struct CollisionInfo {
		const BlockProperties *props = nullptr;
		blockpos_t pos;
		bool is_x = false;
	};
	bool haveOnCollide(const BlockProperties *props) const;
	/// Returns a valid value of BlockProperties::CollisionType
	int onCollide(CollisionInfo ci);


	// -------- Environment
private:
	static int l_world_get_block(lua_State *L);
	static int l_world_set_tile(lua_State *L);


	// -------- Player API
public:
	void setPlayer(Player *player)
	{
		m_player = player;
		m_player_controls_cached = false;
	}
	/// For client-use only
	void setMyPlayer(Player *player)
	{
		m_my_player = player;
	}

private:
	static int l_player_get_pos(lua_State *L);
	static int l_player_set_pos(lua_State *L);
	static int l_player_get_vel(lua_State *L);
	static int l_player_set_vel(lua_State *L);
	static int l_player_get_acc(lua_State *L);
	static int l_player_set_acc(lua_State *L);
	static int l_player_get_controls(lua_State *L);

	Player *m_player = nullptr;
	const Player *m_my_player = nullptr;
	bool m_player_controls_cached = false;


	// -------- Members
private:
	lua_State *m_lua = nullptr;
	BlockManager *m_bmgr = nullptr;
	MediaManager *m_media = nullptr;

	bid_t m_last_block_id = Block::ID_INVALID;
};
