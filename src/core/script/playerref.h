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

private:
	PlayerRef() = default;

	static PlayerRef *toPlayerRef(lua_State *L, int idx);

	/// Cleanup by Lua (dtor)
	static int garbagecollect(lua_State *L);

	static int get_name(lua_State *L);

	Player *m_player;
};

