// SPDX-License-Identifier: LGPL-2.1-or-later

#include "loading.h"
#include "connect.h"
#include "client/client.h"
#include <IGUIButton.h>
#include <IGUIEnvironment.h>
#include <IGUIStaticText.h>

SceneLoading::SceneLoading() :
	SceneHandler(L"<invalid>")
{
}

void SceneLoading::OnOpen()
{
	switch (loading_type) {
		case Type::Invalid: break;
		case Type::ConnectServer:
			SceneHandler::setName(L"Connecting ...");
			break;
		case Type::JoinWorld:
			SceneHandler::setName(L"Joining world ...");
			break;
	}
	m_timer.set(5);
}


void SceneLoading::draw()
{
	auto gui = m_gui->guienv;

	{
		auto rect = m_gui->getRect({20, 40}, {60, 10});
		std::wstring text;

		auto *e = gui->addStaticText(L"Loading ...\nTimeout: 5 seconds", rect);
		e->setOverrideColor(0xFFFFFFFF);
		e->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);
	}

	{
		auto rect = m_gui->getRect({40, 60}, {20, -30});
		gui->addButton(rect, nullptr, -1, L"Cancel");;
	}
}

void SceneLoading::step(float dtime)
{
	// find exit condition
	switch (loading_type) {
		case Type::Invalid:
			m_gui->setSceneFromClientState();
			fprintf(stderr, "SceneLoading: invalid state!\n");
			break;
		case Type::ConnectServer:
			if ((int)m_gui->getClient()->getState() >= (int)ClientState::Connected) {
				// Connected!
				SceneConnect::recordLogin(m_gui->getClient()->getStartData());
				m_gui->setSceneFromClientState();
				break;
			}

			if (m_timer.step(dtime)) {
				m_gui->showPopupText("Connection timed out: Server is not reachable.");
				cancel();
			}
			break;
		case Type::JoinWorld:
			if (m_timer.step(dtime)) {
				m_gui->showPopupText("World join timed out.");
				cancel();
			}
			break;
	}
}

bool SceneLoading::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT
			&& e.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
		// There is only one button.
		cancel();
		return true;
	}
	return false;
}

bool SceneLoading::OnEvent(GameEvent &e)
{
	return false;
}

void SceneLoading::cancel()
{
	switch (loading_type) {
		case Type::Invalid: // if in doubt: disconnect.
		case Type::ConnectServer:
			m_gui->disconnect();
			break;
		case Type::JoinWorld:
			{
				GameEvent e(GameEvent::G2C_LEAVE);
				m_gui->sendNewEvent(e);
				m_gui->setSceneFromClientState();
			}
			break;
	}
}


