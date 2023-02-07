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

static std::string dump_val(const core::vector2df vec)
{
	return "x=" + std::to_string(vec.X)
		+ ", y=" + std::to_string(vec.Y);
}

static std::string dump_val(const core::vector3df vec)
{
	return "x=" + std::to_string(vec.X)
		+ ", y=" + std::to_string(vec.Y)
		+ ", z=" + std::to_string(vec.Z);
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

	// Main node to keep track of all children
	auto stage = smgr->addBillboardSceneNode(nullptr,
		core::dimension2d<f32>(10, 10),
		core::vector3df(0, 0, 0)
	);
	stage->setMaterialFlag(video::EMF_LIGHTING, false);
	stage->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
	stage->setMaterialTexture(0, m_gui->driver->getTexture("assets/textures/dummy.png"));
	m_stage = stage;

	// Set up camera
	m_camera = smgr->addCameraSceneNode(nullptr);
	if (0) {
		core::matrix4 ortho;
		ortho.buildProjectionMatrixOrthoLH(400 * 0.9f, 300 * 0.9f, 0.1f, 1000.0f);
		m_camera->setProjectionMatrix(ortho, true);
		//m_camera->setAspectRatio((float)draw_area.getWidth() / (float)draw_area.getHeight());
	}

	setCamera({60,-60,-200.0f}); // values don't matter
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
	setCamera(pos);
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
				break;
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

					auto pos = m_camera->getPosition();
					pos.Z += dir * 30;
					m_camera->setPosition(pos);
				}
				break;
			case EMIE_LMOUSE_PRESSED_DOWN:
				{
					core::vector2di pos(e.MouseInput.X, e.MouseInput.Y);

					auto shootline = m_gui->scenemgr
							->getSceneCollisionManager()
							->getRayFromScreenCoordinates(pos, m_camera);

					// find clicked element
					for (auto c : m_stage->getChildren()) {
						auto bb = c->getBoundingBox();
						auto abspos = c->getAbsolutePosition();
						bb.MinEdge += abspos;
						bb.MaxEdge += abspos;
						if (bb.intersectsWithLine(shootline)) {
							core::vector3df pos = c->getPosition();

							blockpos_t bp(pos.X / 10.0f, -pos.Y / 10.0f);
							Block b;
							b.id = 11;

							m_gui->getClient()->setBlock(bp, b, 0);
							printf("clicked %s\n", dump_val(pos).c_str());
							break;
						}
					}
				}
				break;
			default: break;
		}
	}
	if (e.EventType == EET_KEY_INPUT_EVENT) {
		auto &controls = m_gui->getClient()->getControls();
		bool down = e.KeyInput.PressedDown;
		EKEY_CODE keycode = e.KeyInput.Key;

		/*switch (e.KeyInput.Char) {
			case L'a': keycode = KEY_LEFT; break;
			case L'd': keycode = KEY_RIGHT; break;
			case L'w': keycode = KEY_UP; break;
			case L's': keycode = KEY_DOWN; break;
		}*/

		// The Client performs physics of all players, including ours.
		switch (keycode) {
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
	World *world = m_gui->getClient()->getWorld();
	if (!world || !m_need_mesh_update)
		return;

	SimpleLock lock(world->mutex);

	m_need_mesh_update = false;

	auto smgr = m_gui->scenemgr;
	m_stage->removeAll();

	auto size = world->getSize();
	for (int x = 0; x < size.X; x++)
	for (int y = 0; y < size.Y; y++) {
		Block b;
		if (!world->getBlock(blockpos_t(x, y), &b))
			continue;

		auto props = g_blockmanager->getProps(b.id);
		if (!props)
			continue;

		// Note: Position is relative to its parent
		auto bb = smgr->addBillboardSceneNode(m_stage,
			core::dimension2d<f32>(10, 10),
			core::vector3df(x * 10, -y * 10, 1)
		);
		bb->setMaterialFlag(video::EMF_LIGHTING, false);
		bb->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
		bb->setMaterialTexture(0, props->texture);

		// Set texture
		auto &mat = bb->getMaterial(0).getTextureMatrix(0);

		float tiles = props->pack->block_ids.size();
		mat.setTextureTranslate(props->texture_offset / tiles, 0);
		mat.setTextureScale(1.0f / tiles, 1);
	}
}

void SceneGameplay::setCamera(core::vector3df pos)
{
	m_camera->setPosition(pos);
	pos.Z += 1E6;
	m_camera->setTarget(pos);
	m_camera->updateAbsolutePosition();
}

