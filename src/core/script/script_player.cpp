#include "script.h"
#include "script_utils.h"
#include "core/player.h"
#include "playerref.h"

namespace {
	class FakePlayer : public Player {
	public:
		FakePlayer() : Player(0) {}
	};
}

static Logger &logger = script_logger;

void Script::setPlayer(Player *player)
{
	*m_player = player;
	m_world = player ? player->getWorld().get() : nullptr;
}

void Script::pushCurrentPlayerRef()
{
	lua_State *L = m_lua;
	lua_getglobal(L, "env");
	{
		FakePlayer p;
		PlayerRef::push(L, &p);
		auto ref = PlayerRef::toPlayerRef(L, -1);
		m_player = ref->ptrRef();
		setPlayer(nullptr);
	}
	lua_setfield(L, -2, "player");

	lua_pop(L, 1); // env
}

void Script::onPlayerEvent(const char *event, Player *player)
{
	setPlayer(player);
	lua_pushstring(m_lua, event);
	runCb_0(m_ref_on_player_event, "on_player_event", 1);
}

void Script::onPlayerEventB(const char *event, Player *player, bool arg)
{
	setPlayer(player);
	lua_pushstring(m_lua, event);
	lua_pushboolean(m_lua, arg);
	runCb_0(m_ref_on_player_event, "on_player_event", 2);
}
