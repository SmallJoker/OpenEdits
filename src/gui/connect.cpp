#include "connect.h"
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIStaticText.h>
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
	core::recti rect_1(
		core::vector2di(100, 100),
		core::dimension2du(150, 30)
	);
	core::recti rect_2 = rect_1 + core::vector2di(120, -5);

	{
		auto rect = rect_2 + core::vector2di(180, 0);

		m_gui->gui->addButton(
			rect, nullptr, ID_BtnConnect, L"Host");
	}

	{
		auto text_a = m_gui->gui->addStaticText(L"Username", rect_1, false, false);
		text_a->setOverrideColor(0xFFFFFFFF);

		m_gui->gui->addEditBox(
			nickname.c_str(), rect_2, true, nullptr, ID_BoxNickname);

		rect_1 += core::vector2di(0, 50);
		rect_2 += core::vector2di(0, 50);
	}

	{
		auto text_a = m_gui->gui->addStaticText(L"Address", rect_1, false, false);
		text_a->setOverrideColor(0xFFFFFFFF);

		core::stringw str;

		m_gui->gui->addEditBox(
			address.c_str(), rect_2, true, nullptr, ID_BoxAddress);

		rect_1 += core::vector2di(0, 50);
		rect_2 += core::vector2di(0, 50);
	}

	m_gui->gui->addButton(
		rect_2, nullptr, ID_BtnConnect, L"Connect");
}

void SceneConnect::step(float dtime)
{
	core::recti rect(200, 10, 200, 30);
	video::SColor color(0xFFFFFFFF);
	video::SColor red(0xFFFF0000);

	m_gui->font->draw(L"Hello world", rect, color);
	if (x >= 0) {
		core::recti rect(x - 6, y - 10, 200, 30);;
		m_gui->font->draw(L"X", rect, red);
	}
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
	if (e.EventType == irr::EET_MOUSE_INPUT_EVENT) {
		switch (e.MouseInput.Event) {
			case EMIE_MOUSE_MOVED:
				x = e.MouseInput.X;
				y = e.MouseInput.Y;
				break;
			default: break;
		}
	}
	return false;
}

bool SceneConnect::OnEvent(const GameEvent &e)
{
	return false;
}


void SceneConnect::onSubmit(int elementid)
{
	auto root = m_gui->gui->getRootGUIElement();

	nickname = root->getElementFromId(ID_BoxNickname)->getText();
	address = root->getElementFromId(ID_BoxAddress)->getText();

	start_localhost = (elementid == ID_BtnHost);

	m_gui->connect(this);
}

