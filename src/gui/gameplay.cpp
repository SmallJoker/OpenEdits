#include "gameplay.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "core/blockmanager.h"
#include "blockselector.h"
#include "minimap.h"
#include <irrlicht.h>

static int SIZEW = 650; // world render size

static float ZINDEX_SMILEY = 0;
static float ZINDEX_LOOKUP[(int)BlockDrawType::Invalid] = {
	0.1, // Solid
	0.1, // Action
	-0.1, // Decoration
	0.2, // Background
};

enum ElementId : int {
	ID_BoxChat = 101,
	ID_BtnBack = 110,
	ID_BtnMinimap,
	ID_ListPlayers = 120,
	ID_PlayerOffset = 300,
};

SceneGameplay::SceneGameplay()
{
	LocalPlayer::gui_smiley_counter = ID_PlayerOffset;
}

SceneGameplay::~SceneGameplay()
{
	if (m_world_smgr) {
		m_world_smgr->clear();
		m_world_smgr->drop();
	}

	delete m_blockselector;
	delete m_minimap;
}

#if 0
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
#endif

// -------------- Public members -------------

void SceneGameplay::OnClose()
{
	m_chathistory_text.clear();
}


void SceneGameplay::draw()
{
	const auto wsize = m_gui->window_size;
	auto gui = m_gui->guienv;

	m_ignore_keys = false;

	{
		SIZEW = wsize.Width * 0.7f;

		m_draw_area = core::recti(
			core::vector2di(1, 1),
			core::dimension2di(SIZEW - 5, wsize.Height - 45)
		);
	}

	// Bottom row
	core::recti rect_1(
		core::vector2di(5, wsize.Height - 35),
		core::dimension2du(100, 30)
	);

	gui->addButton(
		rect_1, nullptr, ID_BtnBack, L"<< Lobby");

	core::recti rect_2(
		core::vector2di(370, wsize.Height - 35),
		core::dimension2du(300, 30)
	);

	gui->addEditBox(
		L"", rect_2, true, nullptr, ID_BoxChat);

	core::recti rect_3(
		core::vector2di(680, wsize.Height - 35),
		core::dimension2du(30, 30)
	);

	gui->addButton(rect_3, nullptr, ID_BtnMinimap, L"M");

	{
		core::recti rect_ch(
			core::vector2di(SIZEW, 160),
			core::dimension2di(wsize.Width - SIZEW, wsize.Height - 50 - 160)
		);
		if (m_chathistory_text.empty())
			m_chathistory_text = L"--- Start of chat history ---\n";

		auto e = gui->addEditBox(m_chathistory_text.c_str(), rect_ch, true);
		e->setAutoScroll(true);
		e->setMultiLine(true);
		e->setWordWrap(true);
		e->setEnabled(false);
		e->setDrawBackground(false);
		e->setTextAlignment(gui::EGUIA_UPPERLEFT, gui::EGUIA_LOWERRIGHT);
		e->setOverrideColor(0xFFCCCCCC);
		m_chathistory = e;
	}

	setupCamera();

	m_dirty_playerlist = true;
	updatePlayerlist();

	{
		// Minimap (below block selector)
		if (!m_minimap)
			m_minimap = new SceneMinimap(this, m_gui);

		m_minimap->draw();
	}

	{
		// Block selector GUI
		if (!m_blockselector)
			m_blockselector = new SceneBlockSelector(gui);

		m_blockselector->setHotbarPos(
			rect_1.UpperLeftCorner + core::position2di(110, 0)
		);
		m_blockselector->draw();
	}
}

void SceneGameplay::step(float dtime)
{
	updateWorld();
	updatePlayerlist();
	updatePlayerPositions(dtime);
	m_blockselector->step(dtime);
	m_minimap->step(dtime);

	{
		// Actually draw the world contents

		auto old_viewport = m_gui->driver->getViewPort();
		m_gui->driver->setViewPort(m_draw_area);
		m_camera->setAspectRatio((float)m_draw_area.getWidth() / m_draw_area.getHeight());

		m_world_smgr->drawAll();

		m_gui->driver->setViewPort(old_viewport);
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
	if (m_blockselector->OnEvent(e))
		return true;

	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				if (e.GUIEvent.Caller->getID() == ID_BtnBack) {
					m_gui->leaveWorld();
					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnMinimap) {
					if (m_minimap)
						m_minimap->toggleVisibility();
					m_gui->guienv->setFocus(nullptr);
					return true;
				}
				break;
			case gui::EGET_EDITBOX_ENTER:
				if (e.GUIEvent.Caller->getID() == ID_BoxChat) {
					auto textw = e.GUIEvent.Caller->getText();

					{
						GameEvent e(GameEvent::G2C_CHAT);
						e.text = new std::string();
						utf32_to_utf8(*e.text, textw);
						m_gui->sendNewEvent(e);
					}

					e.GUIEvent.Caller->setText(L"");
					m_gui->guienv->setFocus(nullptr);
					return true;
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
	if (e.EventType == EET_KEY_INPUT_EVENT) {
		if (e.KeyInput.Key == KEY_LSHIFT || e.KeyInput.Key == KEY_RSHIFT)
			m_erase_mode = e.KeyInput.PressedDown;
	}
	if (e.EventType == EET_MOUSE_INPUT_EVENT) {
		switch (e.MouseInput.Event) {
			case EMIE_MOUSE_MOVED:
			case EMIE_LMOUSE_PRESSED_DOWN:
			case EMIE_RMOUSE_PRESSED_DOWN:
				{
					auto world = m_gui->getClient()->getWorld();
					if (!world || !m_blockselector)
						break;

					// Place currently selected block
					bool l_pressed = e.MouseInput.Event == EMIE_LMOUSE_PRESSED_DOWN;
					// Place bid=0
					bool r_pressed = e.MouseInput.Event == EMIE_RMOUSE_PRESSED_DOWN;
					if (!((m_may_drag_draw && m_drag_draw_block != BLOCKID_INVALID) || l_pressed || r_pressed))
						break;

					blockpos_t bp;
					if (!getBlockFromPixel(e.MouseInput.X, e.MouseInput.Y, bp))
						break;

					bool guess_layer = false;
					if (l_pressed) {
						m_drag_draw_block = m_blockselector->getSelectedBid();
						auto props = g_blockmanager->getProps(m_drag_draw_block);
						if (m_drag_draw_block == 0)
							guess_layer = true;
						else if (props)
							m_drag_draw_block |= (BlockUpdate::BG_FLAG * (props->type == BlockDrawType::Background));
					}

					if (r_pressed || guess_layer) {
						// Update block ID on click
						Block bt;
						if (!world->getBlock(bp, &bt))
							break;

						// Pick background if there is no foreground
						m_drag_draw_block = 0 | (BlockUpdate::BG_FLAG * (bt.id == 0));
					}

					BlockUpdate bu;
					bu.pos = bp;
					if (m_erase_mode)
						bu.id = 0 | (m_drag_draw_block & BlockUpdate::BG_FLAG);
					else
						bu.id = m_drag_draw_block;

					if (!m_may_drag_draw)
						m_drag_draw_block = BLOCKID_INVALID;

					m_gui->getClient()->updateBlock(bu);
					return true;
				}
				break;
			case EMIE_MOUSE_WHEEL:
				{
					core::vector2di pos(e.MouseInput.X, e.MouseInput.Y);
					auto root = m_gui->guienv->getRootGUIElement();
					auto element = root->getElementFromPoint(pos);
					if (element && element != root) {
						// Forward inputs to the corresponding element
						return false;
					}
				}

				{
					float dir = e.MouseInput.Wheel > 0 ? 1 : -1;

					m_camera_pos.Z *= (1 - dir * 0.1);
				}
				break;
			case EMIE_LMOUSE_LEFT_UP:
			case EMIE_RMOUSE_LEFT_UP:
				m_drag_draw_block = BLOCKID_INVALID;
				break;
			default: break;
		}
	}
	if (e.EventType == EET_KEY_INPUT_EVENT && !m_ignore_keys) {
		auto player = m_gui->getClient()->getMyPlayer();
		if (!player)
			return false;

		if (e.KeyInput.Char == L'/') {
			auto root = m_gui->guienv->getRootGUIElement();
			auto element = root->getElementFromId(ID_BoxChat);
			if (element) {
				SEvent ev;
				memcpy(&ev, &e, sizeof(e));
				ev.KeyInput.Key = KEY_END;
				element->OnEvent(ev);

				m_gui->guienv->setFocus(element);
				return true;
			}
		}

		auto controls = player->getControls();
		bool down = e.KeyInput.PressedDown;
		EKEY_CODE keycode = e.KeyInput.Key;

		// The Client performs physics of all players, including ours.
		switch (keycode) {
			case KEY_KEY_A:
			case KEY_LEFT:
				if (down || controls.dir.X < 0)
					controls.dir.X = down ? -1 : 0;
				break;
			case KEY_KEY_D:
			case KEY_RIGHT:
				if (down || controls.dir.X > 0)
					controls.dir.X = down ? 1 : 0;
				break;
			case KEY_KEY_W:
			case KEY_UP:
				if (down || controls.dir.Y < 0)
					controls.dir.Y = down ? -1 : 0;
				break;
			case KEY_KEY_S:
			case KEY_DOWN:
				if (down || controls.dir.Y > 0)
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
			m_dirty_worldmesh = true;

			if (m_minimap)
				m_minimap->markDirty();
			break;
		case E::C2G_PLAYER_JOIN:
			m_dirty_playerlist = true;
			break;
		case E::C2G_PLAYER_LEAVE:
			m_dirty_playerlist = true;
			break;
		case E::C2G_PLAYER_CHAT:
			{
				const char *who = "* SYSTEM";
				if (e.player_chat->player)
					who = e.player_chat->player->name.c_str();

				char buf[200];
				snprintf(buf, sizeof(buf), "%s: %s\n",
					who, e.player_chat->message.c_str()
				);

				std::wstring line;
				utf8_to_utf32(line, buf);
				m_chathistory_text.append(line.c_str());
				m_chathistory->setText(m_chathistory_text.c_str());
			}
			return true;
		default: break;
	}
	return false;
}

bool SceneGameplay::getBlockFromPixel(int x, int y, blockpos_t &bp)
{
	core::vector2di mousepos(x, y);

	if (!m_draw_area.isPointInside(mousepos))
		return false;

	auto old_viewport = m_gui->driver->getViewPort();
	m_gui->driver->setViewPort(m_draw_area);

	auto shootline = m_world_smgr
			->getSceneCollisionManager()
			->getRayFromScreenCoordinates(mousepos, m_camera);

	m_gui->driver->setViewPort(old_viewport);

	/*
		Get X/Y intersection point at Z=0

		dir = end - start  (unit vector is not necessary)
		(x, y, 0) = start + dir * n
		--> n = (0 - start.z) / dir.z
	*/

	auto dir = shootline.end - shootline.start;
	float n = (0.0f - shootline.start.Z) / dir.Z;
	auto xy_point = shootline.start + dir * n;

	// convert to block positions
	xy_point.X = (xy_point.X + 5.0f) / 10.0f;
	xy_point.Y = (-xy_point.Y + 5.0f) / 10.0f;
	//printf("pointed: %f, %f, %f\n", xy_point.X, xy_point.Y, xy_point.Z);

	auto world = m_gui->getClient()->getWorld();
	if (!world->isValidPosition(xy_point.X, xy_point.Y))
		return false;

	bp.X = xy_point.X;
	bp.Y = xy_point.Y;
	return true;
}

video::ITexture *SceneGameplay::generateTexture(const wchar_t *text, u32 color)
{
	auto driver = m_gui->driver;
	auto dim = m_gui->font->getDimension(text);
	dim.Width += 2; dim.Height += 2;

	auto texture = driver->addRenderTargetTexture(dim); //, "rt", video::ECF_A8R8G8B8);
	driver->setRenderTarget(texture); //, true, true, video::SColor(0));

	m_gui->font->draw(text, core::recti(core::vector2di(2,0), dim), 0xFF555555); // Shadow
	m_gui->font->draw(text, core::recti(core::vector2di(1,-1), dim), color);

	driver->setRenderTarget(nullptr, video::ECBF_ALL);

	return texture;
}

void SceneGameplay::updateWorld()
{
	auto world = m_gui->getClient()->getWorld();
	if (!world || !m_dirty_worldmesh)
		return;
	m_dirty_worldmesh = false;

	SimpleLock lock(world->mutex);

	auto smgr = m_gui->scenemgr;

	m_stage->removeAll();

	auto size = world->getSize();
	for (int x = 0; x < size.X; x++)
	for (int y = 0; y < size.Y; y++) {
		Block b;
		if (!world->getBlock(blockpos_t(x, y), &b))
			continue;

		bool have_solid_above = false;
		do {
			if (b.id == 0)
				break;

			const BlockProperties *props = g_blockmanager->getProps(b.id);
			auto z = ZINDEX_LOOKUP[(int)(props ? props->type : BlockDrawType::Solid)];

			// Note: Position is relative to its parent
			auto bb = smgr->addBillboardSceneNode(m_stage,
				core::dimension2d<f32>(10, 10),
				core::vector3df(x * 10, -y * 10, z)
			);
			have_solid_above = assignBlockTexture(props, bb);

		} while (false);


		do {
			if (have_solid_above)
				break;

			const BlockProperties *props = g_blockmanager->getProps(b.bg);
			auto z = ZINDEX_LOOKUP[(int)BlockDrawType::Background];

			// Note: Position is relative to its parent
			auto bb = smgr->addBillboardSceneNode(m_stage,
				core::dimension2d<f32>(10, 10),
				core::vector3df(x * 10, -y * 10, z)
			);
			assignBlockTexture(props, bb);

		} while (false);
	}
}

bool SceneGameplay::assignBlockTexture(const BlockProperties *props, scene::ISceneNode *node)
{
	bool is_opaque = false;

	node->setMaterialFlag(video::EMF_LIGHTING, false);
	node->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
	// Problem: Filtering bleeds into adjacent textures
	node->setMaterialFlag(video::EMF_BILINEAR_FILTER, false);

	if (!props) {
		node->setMaterialTexture(0, g_blockmanager->getMissingTexture());
		return true;
	}

	if (props->type == BlockDrawType::Action)
		node->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
	else if (props->type == BlockDrawType::Decoration)
		node->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL);
	else
		is_opaque = true;

	node->setMaterialTexture(0, props->texture);

	// Set texture
	auto &mat = node->getMaterial(0).getTextureMatrix(0);

	float tiles = props->pack->block_ids.size();
	mat.setTextureTranslate(props->texture_offset / tiles, 0);
	mat.setTextureScale(1.0f / tiles, 1);

	return is_opaque;
}

void SceneGameplay::updatePlayerlist()
{
	auto world = m_gui->getClient()->getWorld();
	if (!m_dirty_playerlist || !world)
		return;

	m_dirty_playerlist = false;

	auto gui = m_gui->guienv;
	auto root = gui->getRootGUIElement();
	auto playerlist = root->getElementFromId(ID_ListPlayers);

	if (playerlist)
		root->removeChild(playerlist);

	const auto wsize = m_gui->window_size;

	core::recti rect(
		core::vector2di(SIZEW, 50),
		core::dimension2du(wsize.Width - SIZEW, 100)
	);

	auto e = gui->addListBox(rect, nullptr, ID_ListPlayers);

	auto list = m_gui->getClient()->getPlayerList();
	for (auto &it : *list.ptr()) {
		core::stringw wstr;
		core::multibyteToWString(wstr, it.second->name.c_str());
		u32 i = e->addItem(wstr.c_str());
		e->setItemOverrideColor(i, Gui::COLOR_ON_BG);
	}

	// Add world ID and online count
	{
		rect.UpperLeftCorner.Y = 5;
		rect.LowerRightCorner.X += 200;
		auto meta = world->getMeta();
		std::string src_text;
		src_text.append("ID: " + meta.id);
		src_text.append("\r\nOwner: " + meta.owner);

		core::stringw dst_text;
		core::multibyteToWString(dst_text, src_text.c_str());

		auto e = gui->addStaticText(dst_text.c_str(), rect);
		e->setOverrideColor(Gui::COLOR_ON_BG);
	}
}

void SceneGameplay::updatePlayerPositions(float dtime)
{
	auto smgr = m_world_smgr;
	auto texture = m_gui->driver->getTexture("assets/textures/smileys.png");
	int texture_tiles = 4; // TODO: add a better check
	if (0) {
		auto dim = texture->getOriginalSize();
		texture_tiles = dim.Width / dim.Height;
	}

	auto cam_pos = m_camera_pos;
	cam_pos.Z = 0;
	do {
		auto me = m_gui->getClient()->getMyPlayer();
		if (!me)
			break;

		if (me->vel.getLengthSQ() < 10 * 10)
			m_nametag_show_timer += dtime;
		else
			m_nametag_show_timer = 0;
	} while (0);

	std::list<scene::ISceneNode *> children = m_players->getChildren();
	auto players = m_gui->getClient()->getPlayerList();
	for (auto it : *players.ptr()) {
		auto player = dynamic_cast<LocalPlayer *>(it.second);

		core::vector3df nf_pos(player->pos.X * 10, player->pos.Y * -10, ZINDEX_SMILEY);
		if (cam_pos.getDistanceFromSQ(nf_pos) > 500 * 500)
			continue; // Do not draw if too far away

		s32 nf_id = player->getGUISmileyId();
		scene::ISceneNode *nf = nullptr;
		for (auto &c : children) {
			if (c && c->getID() == nf_id) {
				nf = c;
				c = nullptr; // mark as handled
			}
		}

		if (nf) {
			nf->setPosition(nf_pos);
		} else {
			nf = smgr->addBillboardSceneNode(m_players,
				core::dimension2d<f32>(10, 10),
				nf_pos,
				nf_id
			);
			nf->setMaterialFlag(video::EMF_LIGHTING, false);
			nf->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
			nf->setMaterialFlag(video::EMF_BILINEAR_FILTER, false);
			nf->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
			nf->setMaterialTexture(0, texture);

			// Set texture
			auto &mat = nf->getMaterial(0).getTextureMatrix(0);
			mat.setTextureTranslate((it.first % texture_tiles) / (float)texture_tiles, 0);
			mat.setTextureScale(1.0f / texture_tiles, 1);


			// Add nametag
			core::stringw namew;
			core::multibyteToWString(namew, it.second->name.c_str());
			auto nt_texture = generateTexture(namew.c_str());
			auto nt_size = nt_texture->getOriginalSize();
			auto nt = smgr->addBillboardSceneNode(nf,
				core::dimension2d<f32>(nt_size.Width * 0.4f, nt_size.Height * 0.4f),
				core::vector3df(0, -10, 0),
				nf_id + 1
			);
			nt->setMaterialFlag(video::EMF_LIGHTING, false);
			nt->setMaterialFlag(video::EMF_ZWRITE_ENABLE, true);
			//nt->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF);
			nt->setMaterialTexture(0, nt_texture);
		}

		for (auto c : nf->getChildren()) {
			auto id = c->getID();
			if (id == nf_id + 1) {
				// Nametag
				c->setVisible(m_nametag_show_timer > 1.0f);
			}
			// id + 2: effect
		}
	}

	for (auto c : children) {
		if (c)
			m_players->removeChild(c);
	}
}


void SceneGameplay::setupCamera()
{
	if (!m_world_smgr) {
		m_world_smgr = m_gui->scenemgr->createNewSceneManager(false);
		//m_world_smgr = m_gui->scenemgr;
	}
	if (m_world_smgr != m_gui->scenemgr)
		m_world_smgr->clear();


	auto smgr = m_world_smgr;

	{
		// Main node to keep track of all children
		auto stage = smgr->addBillboardSceneNode(nullptr,
			core::dimension2d<f32>(0.01f, 0.01f),
			core::vector3df(0, 0, 0)
		);
		m_stage = stage;
	}

	{
		// Main node to keep track of all children
		auto stage = smgr->addBillboardSceneNode(nullptr,
			core::dimension2d<f32>(0.01f, 0.01f),
			core::vector3df(0, 0, 0)
		);
		m_players = stage;
	}

	// Set up camera

	m_camera = smgr->addCameraSceneNode(nullptr);

	if (0) {
		// Makes things worse
		core::matrix4 ortho;
		ortho.buildProjectionMatrixOrthoLH(400 * 0.9f, 300 * 0.9f, 0.1f, 1000.0f);
		m_camera->setProjectionMatrix(ortho, true);
		//m_camera->setAspectRatio((float)draw_area.getWidth() / (float)draw_area.getHeight());
	}

	m_camera_pos.Z = -150.0f;
	setCamera(m_camera_pos);

	m_dirty_worldmesh = true;
}


void SceneGameplay::setCamera(core::vector3df pos)
{
	m_camera->setPosition(pos);
	pos.Z += 1E6;
	m_camera->setTarget(pos);
	m_camera->updateAbsolutePosition();
}

