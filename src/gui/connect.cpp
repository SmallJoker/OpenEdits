#include "connect.h"
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIStaticText.h>
#include <vector2d.h>

enum ElementId : int {
	ID_BoxNickname = 101,
	ID_BoxAddress = 101,
	ID_BtnConnect = 102
};

SceneHandlerConnect::SceneHandlerConnect()
{

}

// -------------- Public members -------------

void SceneHandlerConnect::draw()
{
	core::recti rect_1(
		core::vector2di(100, 100),
		core::dimension2du(150, 30)
	);
	core::recti rect_2 = rect_1 + core::vector2di(120, -5);

	{
		auto text_a = m_render->gui->addStaticText(L"Username", rect_1, false, false);
		text_a->setOverrideColor(0xFFFFFFFF);
		text_a->setOverrideFont(m_render->font);

		auto box_a = m_render->gui->addEditBox(
			L"Guest420", rect_2, true, nullptr, ID_BoxNickname);
		box_a->setOverrideFont(m_render->font);

		rect_1 += core::vector2di(0, 50);
		rect_2 += core::vector2di(0, 50);
	}

	{
		auto text_a = m_render->gui->addStaticText(L"Address", rect_1, false, false);
		text_a->setOverrideColor(0xFFFFFFFF);
		text_a->setOverrideFont(m_render->font);

		auto box_a = m_render->gui->addEditBox(
			L"127.0.0.1", rect_2, true, nullptr, ID_BoxAddress);
		box_a->setOverrideFont(m_render->font);

		rect_1 += core::vector2di(0, 50);
		rect_2 += core::vector2di(0, 50);
	}

	auto btn_c = m_render->gui->addButton(
		rect_2, nullptr, ID_BtnConnect, L"Connect");
	btn_c->setOverrideFont(m_render->font);


}

SceneHandlerType SceneHandlerConnect::step(float dtime)
{
	core::recti rect(200, 10, 200, 30);
	video::SColor color(0xFFFFFFFF);
	video::SColor red(0xFFFF0000);

	m_render->font->draw(L"Hello world", rect, color);
	if (x >= 0 && 0) {
		core::recti rect(x + 2, y, 200, 30);;
		m_render->font->draw(L"MOUSE", rect, red);
	}

	return SceneHandlerType::CTRL_NOOP;
}

bool SceneHandlerConnect::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		if (e.GUIEvent.EventType == gui::EGET_ELEMENT_FOCUSED) {
			core::stringc str;
			wStringToMultibyte(str, e.GUIEvent.Caller->getText());
			printf("Focus: %s\n", str.c_str());
		}
		if (e.GUIEvent.EventType == gui::EGET_EDITBOX_ENTER) {
			core::stringc str;
			wStringToMultibyte(str, e.GUIEvent.Caller->getText());
			printf("Input: %s\n", str.c_str());
		}
	}
	if (e.EventType == irr::EET_MOUSE_INPUT_EVENT) {
		switch (e.MouseInput.Event) {
			case EMIE_LMOUSE_PRESSED_DOWN:
				x = e.MouseInput.X;
				y = e.MouseInput.Y;
			break;

			case EMIE_LMOUSE_LEFT_UP:

			break;

			case EMIE_MOUSE_MOVED:

			break;

			default: break;
		}
	}
	return false;
}
