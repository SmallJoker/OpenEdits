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

void Script::onPlayerJoin(Player *player)
{
	setPlayer(player);
	runCb_0(m_ref_on_player_join, "on_player_join");
}

void Script::onPlayerLeave(Player *player)
{
	setPlayer(player);
	runCb_0(m_ref_on_player_leave, "on_player_leave");
}
