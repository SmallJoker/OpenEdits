#include "gameevent.h"

GameEvent::~GameEvent()
{
	// DO NOT USE THE "DEFAULT:" CASE HERE
	using C = GameEvent::C2G_Enum;
	switch (type_c2g) {
		case C::C2G_DIALOG:
			delete text;
			break;
		case C::C2G_PLAYER_CHAT:
		case C::C2G_LOCAL_CHAT:
			delete player_chat;
			break;
		case C::C2G_ON_TOUCH_BLOCK:
			delete block;
			break;
		// List of no-ops
		case C::C2G_INVALID:
		case C::C2G_DISCONNECT:
		case C::C2G_LOBBY_UPDATE:
		case C::C2G_CHANGE_PASS:
		case C::C2G_JOIN:
		case C::C2G_LEAVE:
		case C::C2G_MAP_UPDATE:
		case C::C2G_META_UPDATE:
		case C::C2G_PLAYER_JOIN:
		case C::C2G_PLAYER_LEAVE:
		case C::C2G_CHAT_HISTORY:
		case C::C2G_PLAYERFLAGS:
			break;
	}

	using G = GameEvent::G2C_Enum;
	switch (type_g2c) {
		case G::G2C_JOIN:
		case G::G2C_CHAT:
		case G::G2C_REGISTER:
			delete text;
			break;
		case G::G2C_CREATE_WORLD:
			delete wc_data;
			break;
		case G::G2C_SET_PASSWORD:
			delete password;
			break;
		// List of no-ops
		case G::G2C_INVALID:
		case G::G2C_LOBBY_REQUEST:
		case G::G2C_LEAVE:
		case G::G2C_GODMODE:
		case G::G2C_SMILEY:
			break;
	}
}

GameEventHandler::~GameEventHandler()
{
	setEventHandler(nullptr);
}

void GameEventHandler::setEventHandler(GameEventHandler *eh)
{
	if (eh == m_eventhandler)
		return;

	GameEventHandler *old_eh = m_eventhandler;
	m_eventhandler = eh;

	if (old_eh)
		old_eh->setEventHandler(nullptr);
	if (eh)
		eh->setEventHandler(this);
}

bool GameEventHandler::sendNewEvent(GameEvent &e)
{
	bool handled = false;
	if (m_eventhandler) {
		if (
				e.type_c2g != GameEvent::C2G_ON_TOUCH_BLOCK && // coin spam
				e.type_c2g != GameEvent::C2G_MAP_UPDATE // (timed) gates spam
			) {
			printf("Processing GameEvent c2g=%d, g2c=%d\n", (int)e.type_c2g, (int)e.type_g2c);
		}
		handled = m_eventhandler->OnEvent(e);
	}

	return handled;
}

