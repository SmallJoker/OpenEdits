#include "connect.h"
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIStaticText.h>
#include <IVideoDriver.h>
#include <vector2d.h>

enum ElementId : int {
	ID_BoxNickname = 101,
	ID_BoxAddress,
	ID_BtnConnect = 110,
	ID_BtnHost
};

SceneConnect::SceneConnect()
{

}

// -------------- Public members -------------

void SceneConnect::draw()
{
	auto gui = m_gui->guienv;

	auto rect_1 = m_gui->getRect({50, 20}, {-160, -30});

	{
		// Logo
		auto texture = gui->getVideoDriver()->getTexture("assets/logo.png");
		auto dim = texture->getOriginalSize();

		core::vector2di pos = rect_1.getCenter() - core::vector2di(rect_1.getWidth() + dim.Width, dim.Height) / 2;
		gui->addImage(texture, pos, false, 0, -1, L"test test");

		rect_1 += core::vector2di(0, dim.Height);
	}


	// Label column
	rect_1 += core::vector2di(-120 * 2, 0);
	// Editbox column
	core::recti rect_2 = rect_1 + core::vector2di(120, -5);


	{
		auto rect = rect_2 + core::vector2di(180, 0);

		gui->addButton(
			rect, nullptr, ID_BtnHost, L"Host");
	}

	{
		auto text_a = gui->addStaticText(L"Username", rect_1, false, false);
		text_a->setOverrideColor(Gui::COLOR_ON_BG);

		gui->addEditBox(
			nickname.c_str(), rect_2, true, nullptr, ID_BoxNickname);

		rect_1 += core::vector2di(0, 50);
		rect_2 += core::vector2di(0, 50);
	}

	{
		auto text_a = gui->addStaticText(L"Address", rect_1, false, false);
		text_a->setOverrideColor(Gui::COLOR_ON_BG);

		core::stringw str;

		gui->addEditBox(
			address.c_str(), rect_2, true, nullptr, ID_BoxAddress);

		rect_1 += core::vector2di(0, 50);
		rect_2 += core::vector2di(0, 50);
	}

	gui->addButton(
		rect_2, nullptr, ID_BtnConnect, L"Connect");
}

void SceneConnect::step(float dtime)
{
}

bool SceneConnect::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				onSubmit(e.GUIEvent.Caller->getID());
				return true;
			default: break;
		}
	}
	return false;
}

bool SceneConnect::OnEvent(GameEvent &e)
{
	return false;
}


void SceneConnect::onSubmit(int elementid)
{
	auto root = m_gui->guienv->getRootGUIElement();

	nickname = root->getElementFromId(ID_BoxNickname)->getText();
	address = root->getElementFromId(ID_BoxAddress)->getText();

	start_localhost = (elementid == ID_BtnHost);

	m_gui->connect(this);
}

