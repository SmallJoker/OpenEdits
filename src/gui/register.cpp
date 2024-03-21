#include "register.h"
#include "client/client.h"
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIStaticText.h>
#include <vector2d.h>

enum ElementId : int {
	ID_BoxPassCurrent = 101,
	ID_BoxPassNew,
	ID_BoxPassConfirm,
	ID_BtnRegister,
	ID_BtnBack
};


SceneRegister::SceneRegister() :
	SceneHandler(L"Register")
{
}


// -------------- Public members -------------

void SceneRegister::draw()
{
	m_is_register = (m_gui->getClient()->getState() == ClientState::Register);

	auto gui = m_gui->guienv;
	auto rect_1 = m_gui->getRect({50, 25}, {-160, -30});

	// Label column
	rect_1 += core::vector2di(-180, 0);
	// Editbox column
	auto rect_2 =  [&rect_1]() {
		return rect_1 + core::vector2di(180, -5);
	};

	const core::vector2di VSPACING(0, 50);

	{
		core::recti rect(
			rect_1.UpperLeftCorner,
			core::dimension2di(500, 100)
		);

		gui::IGUIStaticText *e;
		if (m_is_register) {
			e = gui->addStaticText(
				L"Your account is yet not registered. Please provide password"
				L" for your new account.",
				rect);
		} else {
			e = gui->addStaticText(
				L"Password change prompt",
				rect);
		}
		e->setOverrideColor(Gui::COLOR_ON_BG);

		rect_1 += core::vector2di(0, rect.getHeight());
	}

	if (!m_is_register) {
		// Password change: request the current password

		auto text_a = gui->addStaticText(L"Current password", rect_1, false, false);
		text_a->setOverrideColor(Gui::COLOR_ON_BG);

		auto e = gui->addEditBox(L"", rect_2(), true, nullptr, ID_BoxPassCurrent);
		e->setPasswordBox(true);

		rect_1 += VSPACING;
	}

	{
		auto text_a = gui->addStaticText(L"New password", rect_1, false, false);
		text_a->setOverrideColor(Gui::COLOR_ON_BG);

		auto e = gui->addEditBox(L"", rect_2(), true, nullptr, ID_BoxPassNew);
		e->setPasswordBox(true);

		rect_1 += VSPACING;
	}

	{
		auto text_a = gui->addStaticText(L"Confirm password", rect_1, false, false);
		text_a->setOverrideColor(Gui::COLOR_ON_BG);

		auto e = gui->addEditBox(L"", rect_2(), true, nullptr, ID_BoxPassConfirm);
		e->setPasswordBox(true);

		rect_1 += VSPACING;
	}

	{
		core::recti rect(
			rect_1.UpperLeftCorner - core::vector2di(0, 5),
			core::dimension2di(120, 30)
		);

		auto btn = gui->addButton(rect, nullptr, ID_BtnBack);
		btn->setText(L"<< Back");
	}

	auto btn = gui->addButton(rect_2(), nullptr, ID_BtnRegister);
	if (m_is_register)
		btn->setText(L"Register");
	else
		btn->setText(L"Change password");
}

bool SceneRegister::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				if (e.GUIEvent.Caller->getID() == ID_BtnRegister) {
					if (!m_pass_match) {
						m_gui->showPopupText("Passwords do not match!");
						break;
					}
					auto root = m_gui->guienv->getRootGUIElement();
					auto pass = root->getElementFromId(ID_BoxPassNew);
					if (wcslen(pass->getText()) < 6) {
						m_gui->showPopupText("Password is too short! Use >= 6 characters for your safety.");
						break;
					}

					if (m_is_register) {
						GameEvent ev(GameEvent::G2C_REGISTER);
						ev.text = new std::string();
						wide_to_utf8(*ev.text, pass->getText());

						m_gui->sendNewEvent(ev);
					} else {
						auto pass_curr = root->getElementFromId(ID_BoxPassCurrent);

						GameEvent ev(GameEvent::G2C_SET_PASSWORD);
						ev.password = new GameEvent::PasswordChangeData();
						wide_to_utf8(ev.password->old_pw, pass_curr->getText());
						wide_to_utf8(ev.password->new_pw, pass->getText());

						m_gui->sendNewEvent(ev);
					}

					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnBack) {
					if (m_is_register)
						m_gui->disconnect();
					else
						m_gui->setSceneLoggedIn(SceneHandlerType::Lobby);
					return true;
				}
				break;
			case gui::EGET_EDITBOX_CHANGED:
				if (e.GUIEvent.Caller->getID() == ID_BoxPassNew || e.GUIEvent.Caller->getID() == ID_BoxPassConfirm) {
					auto root = m_gui->guienv->getRootGUIElement();
					auto pass = root->getElementFromId(ID_BoxPassNew);
					auto confirm = (gui::IGUIEditBox *)root->getElementFromId(ID_BoxPassConfirm);

					m_pass_match = wcscmp(pass->getText(), confirm->getText()) == 0;
					if (m_pass_match) {
						// Match
						confirm->enableOverrideColor(false);
					} else {
						// Red highlight
						confirm->setOverrideColor(0xFFDD5555);
					}
				}
				break;
			default: break;
		}
	}
	return false;
}

bool SceneRegister::OnEvent(GameEvent &e)
{
	return false;
}
