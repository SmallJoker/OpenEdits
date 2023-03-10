#include "register.h"
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIStaticText.h>
#include <vector2d.h>

enum ElementId : int {
	ID_BoxEmail = 101,
	ID_BoxEmailConfirm,
	ID_BtnRegister,
	ID_BtnBack
};


// -------------- Public members -------------

void SceneRegister::draw()
{
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

		auto e = gui->addStaticText(
			L"Your account is yet not registered. Please provide an email address"
			L" to receive a temporary password for the first login and subsequent password resets.",
		rect);
		e->setOverrideColor(Gui::COLOR_ON_BG);

		rect_1 += core::vector2di(0, rect.getHeight());
	}

	{
		auto text_a = gui->addStaticText(L"Email address", rect_1, false, false);
		text_a->setOverrideColor(Gui::COLOR_ON_BG);

		gui->addEditBox(L"", rect_2(), true, nullptr, ID_BoxEmail);

		rect_1 += VSPACING;
	}

	{
		auto text_a = gui->addStaticText(L"Confirm email", rect_1, false, false);
		text_a->setOverrideColor(Gui::COLOR_ON_BG);

		gui->addEditBox(L"", rect_2(), true, nullptr, ID_BoxEmailConfirm);

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
	btn->setText(L"Register");
}

bool SceneRegister::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				if (e.GUIEvent.Caller->getID() == ID_BtnRegister) {
					if (!m_email_match) {
						m_gui->showPopupText("Email text does not match!");
						break;
					}
					auto root = m_gui->guienv->getRootGUIElement();
					auto email = root->getElementFromId(ID_BoxEmail);

					GameEvent ev(GameEvent::G2C_REGISTER);
					ev.text = new std::string();
					wide_to_utf8(*ev.text, email->getText());

					m_gui->sendNewEvent(ev);

					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnBack) {
					m_gui->disconnect();
					return true;
				}
				break;
			case gui::EGET_EDITBOX_CHANGED:
				if (e.GUIEvent.Caller->getID() == ID_BoxEmail || e.GUIEvent.Caller->getID() == ID_BoxEmailConfirm) {
					auto root = m_gui->guienv->getRootGUIElement();
					auto email = root->getElementFromId(ID_BoxEmail);
					auto confirm = (gui::IGUIEditBox *)root->getElementFromId(ID_BoxEmailConfirm);

					m_email_match = wcscmp(email->getText(), confirm->getText()) == 0;
					if (m_email_match) {
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
