#include "gameplay.h"
#include "client/client.h"
#include "core/player.h"
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
		core::vector2di(10, 460),
		core::dimension2du(100, 30)
	);

	m_gui->gui->addButton(
		rect_1, nullptr, ID_BtnBack, L"<< Back");

	// Bottom row
	core::recti rect_2(
		core::vector2di(150, 460),
		core::dimension2du(300, 30)
	);

	m_gui->gui->addEditBox(
		L"", rect_2, true, nullptr, ID_BoxChat);

	m_need_mesh_update = true;

	auto smgr = m_gui->scenemgr;

	for (int x = -3; x <= 3; x++)
	for (int y = -3; y <= 3; y++) {
		auto stage = smgr->addBillboardSceneNode(nullptr, core::dimension2d<f32>(10, 10));
		stage->setMaterialFlag(video::EMF_LIGHTING, false);
		stage->setMaterialTexture(0, m_gui->driver->getTexture("assets/textures/dummy.png"));
		stage->setPosition(core::vector3df(x * 10, y * 10, 20));
	}

	m_bb = smgr->addBillboardSceneNode(nullptr, core::dimension2d<f32>(35, 35));
	m_bb->setMaterialFlag(video::EMF_LIGHTING, false);
	m_bb->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
	m_bb->setMaterialTexture(0, m_gui->driver->getTexture("assets/textures/pack_basic.png"));
	auto &mat = m_bb->getMaterial(0).getTextureMatrix(0);
	mat.setTextureTranslate(1.0f / 7, 0);
	mat.setTextureScale(1.0f / 7, 1);

	m_bb->setPosition(core::vector3df(0,0,0));

	m_camera = smgr->addCameraSceneNode(nullptr);

	if (1) {
		core::matrix4 ortho;
		ortho.buildProjectionMatrixOrthoLH(400 * 0.7f, 300 * 0.7f, 0.1f, 1000.0f);
		m_camera->setProjectionMatrix(ortho, true);
		//m_camera->setAspectRatio((float)draw_area.getWidth() / (float)draw_area.getHeight());
	}

	m_camera->setPosition({0,0,-50.0f});
	m_camera->setTarget({0,0,1E6});
	//printf("x=%f,y=%f,z=%f\n", vec.X, vec.Y, vec.Z);
}

void SceneGameplay::step(float dtime)
{
	core::recti rect(200, 10, 200, 30);
	video::SColor color(0xFFFFFFFF);

	m_gui->font->draw(L"Gameplay", rect, color);

	drawWorld();

	if (m_gui->getClient()->getState() == ClientState::LobbyIdle) {
		GameEvent e(GameEvent::G2C_JOIN);
		e.text = new std::string("dummyworld");
		m_gui->sendNewEvent(e);
	}

	auto &controls = m_gui->getClient()->getControls();
	auto pos = m_camera->getPosition();
	pos.X += controls.direction.X * 500 * dtime;
	pos.Y += controls.direction.Y * 500 * dtime;
	m_camera->setPosition(pos);
	m_camera->updateAbsolutePosition();
}

bool SceneGameplay::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				if (e.GUIEvent.Caller->getID() == ID_BtnBack)
					m_gui->leaveWorld();
				break;
			case gui::EGET_EDITBOX_ENTER:
				if (e.GUIEvent.Caller->getID() == ID_BoxChat) {
					auto textw = e.GUIEvent.Caller->getText();

					{
						GameEvent e(GameEvent::G2C_CHAT);
						e.text = new std::string();
						wStringToMultibyte(*e.text, textw);
						m_gui->sendNewEvent(e);
					}

					e.GUIEvent.Caller->setText(L"");
				}
			default: break;
		}
	}
	if (e.EventType == EET_MOUSE_INPUT_EVENT) {

		switch (e.MouseInput.Event) {
			case EMIE_MOUSE_MOVED:
				break;
			case EMIE_MOUSE_WHEEL:
				{
					float dir = e.MouseInput.Wheel > 0 ? 1 : -1;

					auto &mat = m_bb->getMaterial(0).getTextureMatrix(0);
					float x, y;
					mat.getTextureTranslate(x, y);
					x += dir / 7.0f;
					mat.setTextureTranslate(x, 0);
				}
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
	using E = GameEvent::C2G_Enum;

	switch (e.type_c2g) {
		case E::C2G_MAP_UPDATE:
			printf(" * Map update\n");
			m_need_mesh_update = true;
			break;
		case E::C2G_PLAYER_JOIN:
			printf(" * Player %s joined\n",
				e.player->name.c_str()
			);
			break;
		case E::C2G_PLAYER_LEAVE:
			printf(" * Player %s left\n",
				e.player->name.c_str()
			);
			break;
		case E::C2G_PLAYER_CHAT:
			printf(" * <%s> %s\n",
				e.player_chat->player->name.c_str(),
				e.player_chat->message.c_str()
			);
			break;
		default: break;
	}
	return false;
}

void SceneGameplay::drawWorld()
{
	if (true)
		return;

	World *world = m_gui->getClient()->getWorld();
	if (!world || !m_need_mesh_update)
		return;

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

