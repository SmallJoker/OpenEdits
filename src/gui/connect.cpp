#include "connect.h"
#include "IGUIFont.h"

SceneHandlerConnect::SceneHandlerConnect()
{

}

// -------------- Public members -------------

SceneHandlerType SceneHandlerConnect::runPre(float dtime)
{
	core::recti rect(200, 10, 200, 30);
	video::SColor color(0xFFFFFFFF);
	video::SColor red(0xFFFF0000);

	m_render->font->draw(L"Hello world", rect, color);
	if (x >= 0) {
		core::recti rect(x, y, 200, 30);;
		m_render->font->draw(L"MOUSE", rect, red);
	}
	return SceneHandlerType::KEEP_PREVIOUS;
}

void SceneHandlerConnect::runPost()
{
}

bool SceneHandlerConnect::OnEvent(const SEvent &e)
{
	if (e.EventType == irr::EET_MOUSE_INPUT_EVENT) {
		switch (e.MouseInput.Event) {
			case EMIE_LMOUSE_PRESSED_DOWN:
				x = e.MouseInput.X;
				y = e.MouseInput.Y;
			return true;

			case EMIE_LMOUSE_LEFT_UP:

			break;

			case EMIE_MOUSE_MOVED:

			break;

			default: break;
		}
	}
	return false;
}
