#include "gameplay.h"
#include "client/client.h"
#include "client/localplayer.h"
#include <irrlicht.h>

enum ElementId : int {
	ID_BoxChat = 101,
	ID_BtnBack = 110,
	ID_ListPlayers = 120,
};

SceneGameplay::SceneGameplay()
{
}

SceneGameplay::~SceneGameplay()
{
	//m_world_smgr->drop();
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
	const auto wsize = m_gui->window_size;

	// Bottom row
	core::recti rect_1(
		core::vector2di(10, wsize.Height - 40),
		core::dimension2du(100, 30)
	);

	m_gui->gui->addButton(
		rect_1, nullptr, ID_BtnBack, L"<< Back");

	core::recti rect_2(
		core::vector2di(150, wsize.Height - 40),
		core::dimension2du(300, 30)
	);

	m_gui->gui->addEditBox(
		L"", rect_2, true, nullptr, ID_BoxChat);

	{
		core::recti rect_ch(
			core::vector2di(600, 160),
			core::dimension2di(wsize.Width - 600, wsize.Height - 50 - 160)
		);
		if (m_chathistory_text.empty())
			m_chathistory_text = L"Chat history:\n";

		auto e = m_gui->gui->addEditBox(m_chathistory_text.c_str(), rect_ch, true);
		e->setAutoScroll(true);
		e->setMultiLine(true);
		e->setWordWrap(true);
		e->setEnabled(false);
		e->setDrawBackground(false);
		e->setTextAlignment(gui::EGUIA_UPPERLEFT, gui::EGUIA_UPPERLEFT);
		e->setOverrideColor(0xFFCCCCCC);
		m_chathistory = e;
	}

	setupCamera();

	m_need_playerlist_update = true;
}

void SceneGameplay::step(float dtime)
{
	updateWorld();
	updatePlayerlist();
	updatePlayerPositions();

	if (0) {
		// Actually draw the world contents
		// Disabled: conflicts with the collision manager
		static const core::recti draw_area(
			core::vector2di(1, 1),
			core::dimension2di(600 - 5, 500)
		);

		auto old_viewport = m_gui->driver->getViewPort();
		m_gui->driver->setViewPort(draw_area);

		m_world_smgr->drawAll();

		m_gui->driver->setViewPort(old_viewport);
	}

	if (m_gui->getClient()->getState() == ClientState::LobbyIdle) {
		GameEvent e(GameEvent::G2C_JOIN);
		e.text = new std::string("dummyworld");
		m_gui->sendNewEvent(e);
	}

	if (0) {
		auto player = m_gui->getClient()->getMyPlayer();
		auto controls = player->getControls();
		auto pos = m_camera->getPosition();
		pos.X += controls.dir.X * 500 * dtime;
		pos.Y += controls.dir.Y * 500 * dtime;
		setCamera(pos);
	}

	do {
		auto player = m_gui->getClient()->getMyPlayer();
		if (!player)
			break;

		m_camera_pos.X += ((player->pos.X *  10) - m_camera_pos.X) * 5 * dtime;
		m_camera_pos.Y += ((player->pos.Y * -10) - m_camera_pos.Y) * 5 * dtime;

		setCamera(m_camera_pos);
	} while (false);
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
			case gui::EGET_ELEMENT_FOCUSED:
				m_ignore_keys = true;
				break;
			case gui::EGET_ELEMENT_FOCUS_LOST:
				m_ignore_keys = false;
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

					m_camera_pos.Z += dir * 30;
				}
				break;
			case EMIE_LMOUSE_PRESSED_DOWN:
				{
					blockpos_t bp = getBlockFromPixel(e.MouseInput.X, e.MouseInput.Y);
					Block b;
					b.id = 11;
					m_gui->getClient()->setBlock(bp, b, 0);
				}
				break;
			case EMIE_RMOUSE_PRESSED_DOWN:
				{
					blockpos_t bp = getBlockFromPixel(e.MouseInput.X, e.MouseInput.Y);
					Block b;
					b.id = 0;
					m_gui->getClient()->setBlock(bp, b, 0);
				}
				break;
			default: break;
		}
	}
	if (e.EventType == EET_KEY_INPUT_EVENT && !m_ignore_keys) {
		auto player = m_gui->getClient()->getMyPlayer();
		if (!player)
			return false;

		auto controls = player->getControls();
		bool down = e.KeyInput.PressedDown;
		EKEY_CODE keycode = e.KeyInput.Key;

		// The Client performs physics of all players, including ours.
		switch (keycode) {
			case KEY_KEY_A:
			case KEY_LEFT:
				controls.dir.X = down ? -1 : 0;
				break;
			case KEY_KEY_D:
			case KEY_RIGHT:
				controls.dir.X = down ? 1 : 0;
				break;
			case KEY_KEY_W:
			case KEY_UP:
				controls.dir.Y = down ? -1 : 0;
				break;
			case KEY_KEY_S:
			case KEY_DOWN:
				controls.dir.Y = down ? 1 : 0;
				break;
			case KEY_SPACE:
				controls.jump = down;
				break;
			case KEY_KEY_R:
				player->pos = core::vector2df(2, 0);
				player->vel = core::vector2df(0, 0);
				break;
			default: break;
		}

		controls.dir = controls.dir.normalize();
		bool changed = player->setControls(controls);

		if (changed) {
			player.release();
			m_gui->getClient()->sendPlayerMove();
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
			m_need_playerlist_update = true;
			break;
		case E::C2G_PLAYER_LEAVE:
			printf(" * Player %s left\n",
				e.player->name.c_str()
			);
			m_need_playerlist_update = true;
			break;
		case E::C2G_PLAYER_CHAT:
			printf(" * <%s> %s\n",
				e.player_chat->player->name.c_str(),
				e.player_chat->message.c_str()
			);
			{
				char buf[200];
				snprintf(buf, sizeof(buf), "%s: %s\n",
					e.player_chat->player->name.c_str(),
					e.player_chat->message.c_str()
				);

				core::stringw line;
				core::multibyteToWString(line, buf);
				m_chathistory_text.append(line);
				m_chathistory->setText(m_chathistory_text.c_str());
			}
			break;
		default: break;
	}
	return false;
}

blockpos_t SceneGameplay::getBlockFromPixel(int x, int y)
{
	core::vector2di mousepos(x, y);

	auto shootline = m_world_smgr
			->getSceneCollisionManager()
			->getRayFromScreenCoordinates(mousepos, m_camera);

	// find clicked element
	for (auto c : m_stage->getChildren()) {
		auto bb = c->getBoundingBox();
		auto abspos = c->getAbsolutePosition();
		bb.MinEdge += abspos;
		bb.MaxEdge += abspos;
		if (bb.intersectsWithLine(shootline)) {
			core::vector3df pos = c->getPosition();

			//printf("clicked %s\n", dump_val(pos).c_str());
			return blockpos_t((pos.X + 0.5f) / 10.0f, (-pos.Y + 0.5f) / 10.0f);
		}
	}

	return blockpos_t(-1, -1);
}


void SceneGameplay::updateWorld()
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

void SceneGameplay::updatePlayerlist()
{
	if (!m_need_playerlist_update)
		return;

	m_need_playerlist_update = false;

	auto root = m_gui->gui->getRootGUIElement();
	auto playerlist = root->getElementFromId(ID_ListPlayers);

	if (playerlist)
		root->removeChild(playerlist);

	const auto wsize = m_gui->window_size;

	core::recti rect(
		core::vector2di(600, 50),
		core::dimension2du(wsize.Width - 600, 100)
	);

	auto e = m_gui->gui->addListBox(rect, nullptr, ID_ListPlayers);

	auto list = m_gui->getClient()->getPlayerList();
	for (auto &it : *list) {
		core::stringw wstr;
		core::multibyteToWString(wstr, it.second->name.c_str());
		u32 i = e->addItem(wstr.c_str());
		e->setItemOverrideColor(i, 0xFFFFFFFF);
	}
}

void SceneGameplay::updatePlayerPositions()
{
	m_players->removeAll();

	auto smgr = m_world_smgr;
	auto texture = m_gui->driver->getTexture("assets/textures/smileys.png");

	auto players = m_gui->getClient()->getPlayerList();
	for (auto it : *players) {
		auto pos = it.second->pos;

		auto bb = smgr->addBillboardSceneNode(m_players,
			core::dimension2d<f32>(10, 10),
			core::vector3df(pos.X * 10, -pos.Y * 10, 0.0f)
		);
		bb->setMaterialFlag(video::EMF_LIGHTING, false);
		bb->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
		bb->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
		bb->setMaterialTexture(0, texture);

		// Set texture
		auto &mat = bb->getMaterial(0).getTextureMatrix(0);
		int tiles = 4;
		mat.setTextureTranslate((it.first % tiles) / (float)tiles, 0);
		mat.setTextureScale(1.0f / tiles, 1);
	}
}


void SceneGameplay::setupCamera()
{
	if (!m_world_smgr) {
		//m_world_smgr = m_gui->scenemgr->createNewSceneManager(false);
		m_world_smgr = m_gui->scenemgr;
	}

	auto smgr = m_world_smgr;
	auto txt_dummy = m_gui->driver->getTexture("assets/textures/dummy.png");

	{
		// Main node to keep track of all children
		auto stage = smgr->addBillboardSceneNode(nullptr,
			core::dimension2d<f32>(5, 5),
			core::vector3df(0, 0, 0)
		);
		stage->setMaterialFlag(video::EMF_LIGHTING, false);
		stage->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
		stage->setMaterialTexture(0, txt_dummy);
		m_stage = stage;
	}

	{
		// Main node to keep track of all children
		auto stage = smgr->addBillboardSceneNode(nullptr,
			core::dimension2d<f32>(5, 5),
			core::vector3df(0, 0, 0)
		);
		stage->setMaterialFlag(video::EMF_LIGHTING, false);
		stage->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
		stage->setMaterialTexture(0, txt_dummy);
		m_players = stage;
	}

	// Set up camera

	m_camera = smgr->addCameraSceneNode(nullptr);
	/*core::matrix4 mx = core::IdentityMatrix;
	mx.setTranslation({100,0,0});*/

	if (0) {
		// Makes things worse
		core::matrix4 ortho;
		ortho.buildProjectionMatrixOrthoLH(400 * 0.9f, 300 * 0.9f, 0.1f, 1000.0f);
		m_camera->setProjectionMatrix(ortho, true);
		//m_camera->setAspectRatio((float)draw_area.getWidth() / (float)draw_area.getHeight());
	}

	setCamera({60,-60,-150.0f});
	m_camera_pos = m_camera->getPosition();

	m_need_mesh_update = true;
}


void SceneGameplay::setCamera(core::vector3df pos)
{
	m_camera->setPosition(pos);
	pos.Z += 1E6;
	m_camera->setTarget(pos);
	m_camera->updateAbsolutePosition();
}

