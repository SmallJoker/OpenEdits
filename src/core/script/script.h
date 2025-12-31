#pragma once

#include "core/operators.h" // PositionRange
#include "core/types.h" // bid_t
#include <string>
#include <vector>

struct BlockParams;
struct BlockProperties;
struct lua_State;
struct SmileyDef;
class BlockManager;
class MediaManager;
class Player;
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

	void setMediaMgr(MediaManager *media) { m_media = media; }
	/// Safe file loader
	bool loadFromAsset(const std::string &asset_name);

	virtual bool isElevated() const = 0;
	virtual Player *getMyPlayer() const = 0;

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
	static int l_register_smileys(lua_State *L);
	static int l_register_pack(lua_State *L);
	static int l_change_block(lua_State *L);

public:
	/// May return an empty vector in case no smileys were registered.
	const std::vector<SmileyDef> &getSmileys() const { return m_smileys; }
private:
	std::vector<SmileyDef> m_smileys;

	int m_private_include_depth = 0;

	// -------- Callbacks
public:
	virtual void onScriptsLoaded();

	void onStep(double abstime);

	void onIntersect(const BlockProperties *props);
	void onIntersectOnce(blockpos_t pos, const BlockProperties *props);

	struct CollisionInfo {
		const BlockProperties *props = nullptr;
		int tiletype;
		blockpos_t pos;
		bool is_x = false;
	};
	/// Returns a valid value of BlockProperties::CollisionType
	int onCollide(CollisionInfo ci);
protected:
	/// Returns `true` on success
	/// On success with `nargs > 0`, `lua_settop` must be called manually.
	/// The returned arguments are at -nargs, -nargs+1, -nargs+2, ...
	bool callFunction(int ref, int nres, const char *dbg, int nargs, bool is_block = false);


	// -------- World / events
protected:
	void getPositionRange(int idx, PositionRange &range);

public:
	union EventDest {
		Player *player;
		World *world;
	};

	/// `is_player == false`: provide `edst.world`. args start at #1.
	/// `is_player == true`: provide `edst.player`. args start at #2.
	void implSendEvent(EventDest edst, bool is_player);

	/// returns how many values were read
	static int readBlockParams(lua_State *L, int idx, BlockParams &params);
	/// returns the amount of pushed values
	static int writeBlockParams(lua_State *L, const BlockParams &params);

protected:
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
	void removePlayer(Player *player);
	void onPlayerEvent(const char *event, Player *player);
	void onPlayerEventB(const char *event, Player *player, bool arg);

protected:
	void pushCurrentPlayerRef();
	inline Player *getCurrentPlayer() const { return m_player ? *m_player : nullptr; }
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
	int m_ref_on_player_event = -2; // LUA_NOREF

	bool m_loading_complete = false;
};
