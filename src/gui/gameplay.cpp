#include "gameplay.h"
#include "client/client.h"
#include <irrlicht.h>

enum ElementId : int {
	ID_BoxChat = 101,
	ID_BtnBack = 110,
};

SceneGameplay::SceneGameplay()
{
	camera_pos = core::vector2df(0, 0);

	draw_area = core::recti(
		core::vector2di(0, 60),
		core::dimension2di(500, 300)
	);
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

	drawWorld();
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
	if (e.EventType == EET_MOUSE_INPUT_EVENT) {
		switch (e.MouseInput.Event) {
			case EMIE_MOUSE_MOVED:
				break;
			default: break;
		}
	}
	if (e.EventType == EET_KEY_INPUT_EVENT) {
		auto &controls = m_gui->getClient()->getControls();
		bool down = e.KeyInput.PressedDown;

		// The Client performs physics of all players, including ours.
		switch (e.KeyInput.Key) {
			case KEY_LEFT:
				controls.direction.X = down ? -1 : 0;
				break;
			case KEY_RIGHT:
				controls.direction.X = down ? 1 : 0;
				break;
			case KEY_UP:
				controls.direction.Y = down ? 1 : 0;
				break;
			case KEY_DOWN:
				controls.direction.Y = down ? -1 : 0;
				break;
			case KEY_SPACE:
				controls.jump = down;
				break;
			default: break;
		}
	}
	return false;
}

bool SceneGameplay::OnEvent(GameEvent &e)
{
	return false;
}

void SceneGameplay::drawWorld()
{
	//World *world = m_gui->getClient()->getWorld();
	video::ITexture *image = m_gui->driver->getTexture("assets/textures/dummy.png");
    m_gui->driver->makeColorKeyTexture(image, core::position2di(0,0));

	//auto extent = draw_area.getSize() / 2;
	auto size = image->getSize();

	for (s32 y = draw_area.UpperLeftCorner.Y; y < draw_area.LowerRightCorner.Y; y += size.Height)
	for (s32 x = draw_area.UpperLeftCorner.X; x < draw_area.LowerRightCorner.X; x += size.Width) {
		int pos_x = (x - draw_area.UpperLeftCorner.X) / size.Width;
		int pos_y = (y - draw_area.UpperLeftCorner.Y) / size.Height;

		m_gui->driver->draw2DImage(image, core::position2di(x, y), false);
	}
}

