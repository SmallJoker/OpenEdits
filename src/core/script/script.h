#pragma once

#include "core/operators.h" // PositionRange
#include "core/types.h" // bid_t
#include <string>

struct BlockProperties;
struct lua_State;
struct ScriptEvent;
class BlockManager;
class MediaManager;
class Packet;
class Player;
class PlayerRef;
class ScriptEventManager;
class World;

class Script {
public:
	enum Type {
		ST_UNKNOWN,
		ST_SERVER,
		ST_CLIENT
	};

	Script(BlockManager *bmgr, Type type);
	virtual ~Script();

	bool init();
	void close();
	Type getScriptType() const { return m_scripttype; }
	lua_State *getState() const { return m_lua; }
	const BlockManager *getBlockMgr() const { return m_bmgr; }
	ScriptEventManager *getSEMgr() const { return m_emgr; }
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
	/// Getter for `env.test_feedback` (unittests)
	std::string popTestFeedback();
	static int popErrorCount();

	// UNSAFE. Accepts any path.
	/// returns true on success
	bool loadFromFile(const std::string &filename);

	bool hide_global_table = true;

	// -------- Registration
protected:
	static int l_load_hardcoded_packs(lua_State *L);
	/// Includes another script file (asset from cache or disk)
	static int l_include(lua_State *L);
	static int l_require_asset(lua_State *L);
	static int l_register_pack(lua_State *L);
	static int l_change_block(lua_State *L);

	int m_private_include_depth = 0;

	// -------- Callbacks
public:
	virtual void onScriptsLoaded();

	void onStep(double abstime);

	void onBlockPlaced(bid_t block_id);
	//void onBlockErased(bid_t block_id);

	void onIntersect(const BlockProperties *props);
	void onIntersectOnce(blockpos_t pos, const BlockProperties *props);

	struct CollisionInfo {
		const BlockProperties *props = nullptr;
		blockpos_t pos;
		bool is_x = false;
	};
	/// Returns a valid value of BlockProperties::CollisionType
	int onCollide(CollisionInfo ci);
protected:
	void runCb_0(int ref, const char *dbg);
	void runBlockCb_0(int ref, const char *dbg);


	// -------- World / events
protected:
	static void get_position_range(lua_State *L, int idx, PositionRange &range);

	virtual int implWorldSetTile(PositionRange range, bid_t block_id, int tile) = 0;


protected:
	static int l_register_event(lua_State *L);
	static int l_send_event(lua_State *L);

	static int l_world_get_block(lua_State *L);
	static int l_world_get_blocks_in_range(lua_State *L);
	static int l_world_get_params(lua_State *L);
	static int l_world_set_tile(lua_State *L);


	// -------- Player API
public:
	void setPlayer(Player *player);
	void setWorld(World *world)
	{
		*m_player = nullptr;
		m_world = world;
	}
	void onPlayerJoin(Player *player);
	void onPlayerLeave(Player *player);

protected:
	void pushCurrentPlayerRef();
	inline Player *getCurrentPlayer() const { return *m_player; }
	Player **m_player = nullptr;


	// -------- Members
protected:
	const Type m_scripttype;

	lua_State *m_lua = nullptr;
	BlockManager *m_bmgr = nullptr;
	MediaManager *m_media = nullptr;
	World *m_world = nullptr;

	ScriptEventManager *m_emgr = nullptr; // owned

	bid_t m_last_block_id = Block::ID_INVALID;

	int m_ref_on_step = -2; // LUA_NOREF
	int m_ref_on_player_join = -2; // LUA_NOREF
	int m_ref_on_player_leave = -2; // LUA_NOREF
};
