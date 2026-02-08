#pragma once

class Player;
struct lua_State;

class PlayerRef
{
public:
	static void doRegister(lua_State *L);

	static int push(lua_State *L, Player *player);

	/// Cleanup by C++ (empty container)
	static bool invalidate(lua_State *L, Player *player);

	inline Player **ptrRef() { return &m_player; }

	static PlayerRef *toPlayerRef(lua_State *L, int idx);

private:
	PlayerRef() = default;

	/// Cleanup by Lua (dtor)
	static int garbagecollect(lua_State *L);

	static int get_name(lua_State *L);
	static int hash(lua_State *L);

	static int send_event(lua_State *L);
	static int next_prn(lua_State *L);

	// Physics / controls
	static int get_pos(lua_State *L);
	static int set_pos(lua_State *L);
	static int get_vel(lua_State *L);
	static int set_vel(lua_State *L);
	static int get_acc(lua_State *L);
	static int set_acc(lua_State *L);
	static int get_controls(lua_State *L);

	// TODO: Cannot have more members here because 'env.player' is a different
	// instance than e.g. returned by a getter function.
	Player *m_player;
};

