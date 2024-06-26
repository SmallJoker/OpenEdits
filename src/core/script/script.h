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
	virtual ~Script();

	bool init();
	void close();
	virtual void step(float dtime) {}

	void setMediaMgr(MediaManager *media) { m_media = media; }
	/// Safe file loader
	bool loadFromAsset(const std::string &asset_name);

protected:
	virtual void initSpecifics() {}
	virtual void closeSpecifics() {}

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
	static int l_require_asset(lua_State *L);
	static int l_register_pack(lua_State *L);
	static int l_change_block(lua_State *L);


	// -------- Callbacks
public:
	void onScriptsLoaded();
	void onIntersect(const BlockProperties *props);
	void onIntersectOnce(const BlockProperties *props);

	struct CollisionInfo {
		const BlockProperties *props = nullptr;
		blockpos_t pos;
		bool is_x = false;
	};
	/// Returns a valid value of BlockProperties::CollisionType
	int onCollide(CollisionInfo ci);


	// -------- Environment
public:
	void onEvent(blockpos_t pos, bid_t block_id, uint32_t payload);

protected:
	static void get_position_range(lua_State *L, int idx, PositionRange &range);

	virtual int implWorldSetTile(PositionRange range, bid_t block_id, int tile) = 0;


private:
	static int l_world_event(lua_State *L);
	static int l_world_get_block(lua_State *L);
	static int l_world_get_params(lua_State *L);
	static int l_world_set_tile(lua_State *L);


	// -------- Player API
public:
	void setPlayer(Player *player)
	{
		m_player = player;
		m_player_controls_cached = false;
	}

private:
	static int l_player_get_pos(lua_State *L);
	static int l_player_set_pos(lua_State *L);
	static int l_player_get_vel(lua_State *L);
	static int l_player_set_vel(lua_State *L);
	static int l_player_get_acc(lua_State *L);
	static int l_player_set_acc(lua_State *L);
	static int l_player_get_controls(lua_State *L);
protected:
	Player *m_player = nullptr;
	bool m_player_controls_cached = false;


	// -------- Members
protected:
	lua_State *m_lua = nullptr;
	BlockManager *m_bmgr = nullptr;
	MediaManager *m_media = nullptr;

	int m_ref_event_handler = -2; // LUA_NOREF

	bid_t m_last_block_id = Block::ID_INVALID;
};
