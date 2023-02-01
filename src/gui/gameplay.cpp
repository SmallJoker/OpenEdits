#include "gameplay.h"
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIStaticText.h>
#include <vector2d.h>

enum ElementId : int {
	ID_BoxChat = 101,
	ID_BtnBack = 110,
};

SceneGameplay::SceneGameplay()
{

}

// -------------- Public members -------------

void SceneGameplay::draw()
{
	core::recti rect_1(
		core::vector2di(10, 10),
		core::dimension2du(100, 30)
	);

	m_gui->gui->addButton(
		rect_1, nullptr, ID_BtnBack, L"<< Back");

	// Bottom row
	core::recti rect_2(
		core::vector2di(100, 400),
		core::dimension2du(300, 30)
	);

	m_gui->gui->addEditBox(
		L"", rect_2, true, nullptr, ID_BoxChat);

}

void SceneGameplay::step(float dtime)
{
	core::recti rect(200, 10, 200, 30);
	video::SColor color(0xFFFFFFFF);

	m_gui->font->draw(L"Gameplay", rect, color);
}

bool SceneGameplay::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				if (e.GUIEvent.Caller->getID() == ID_BtnBack)
					m_gui->leaveWorld();
				break;
			default: break;
		}
	}
	if (e.EventType == irr::EET_MOUSE_INPUT_EVENT) {
		switch (e.MouseInput.Event) {
			case EMIE_MOUSE_MOVED:
				break;
			default: break;
		}
	}
	return false;
}

bool SceneGameplay::OnEvent(const GameEvent &e)
{
	return false;
}

