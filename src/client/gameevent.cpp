#include "gameevent.h"

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
		printf("Processing GameEvent c2g=%d, g2c=%d\n", (int)e.type_c2g, (int)e.type_g2c);
		handled = m_eventhandler->OnEvent(e);
	}

	// DO NOT USE THE "DEFAULT:" CASE HERE
	using C = GameEvent::C2G_Enum;
	switch (e.type_c2g) {
		case C::C2G_DIALOG:
			delete e.text;
			break;
		case C::C2G_PLAYER_CHAT:
			delete e.player_chat;
			break;
		// List of no-ops
		case C::C2G_INVALID:
		case C::C2G_DISCONNECT:
		case C::C2G_MAP_UPDATE:
		case C::C2G_PLAYER_JOIN:
		case C::C2G_PLAYER_LEAVE:
			break;
	}

	using G = GameEvent::G2C_Enum;
	switch (e.type_g2c) {
		case G::G2C_JOIN:
		case G::G2C_CHAT:
			delete e.text;
			break;
		case G::G2C_SET_BLOCK:
			delete e.block;
			break;
		// List of no-ops
		case G::G2C_INVALID:
		case G::G2C_LEAVE:
			break;
	}

	return handled;
}

