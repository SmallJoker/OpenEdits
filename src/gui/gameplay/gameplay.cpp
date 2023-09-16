#include "gameplay.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "core/blockmanager.h"
#include "blockselector.h"
#include "minimap.h"
#include "smileyselector.h"
#include "world_render.h"
#include <irrlicht.h>

static int SIZEW = 650; // world render size

enum ElementId : int {
	ID_BoxChat = 101,
	ID_BtnChat,
	ID_BtnBack = 110,
	ID_BtnGodMode,
	ID_BtnMinimap,
	ID_LabelTitle,
	ID_ListPlayers = 120
};

SceneGameplay::SceneGameplay() :
	m_drag_draw_block(g_blockmanager)
{
	LocalPlayer::gui_smiley_counter = 300; // Unique ID for the player SceneNode
}

SceneGameplay::~SceneGameplay()
{
	delete m_blockselector;
	delete m_minimap;
	delete m_smileyselector;
	delete m_world_render;
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

void SceneGameplay::OnOpen()
{
	m_gamecmd.initialize(m_gui->getClient());

	m_drag_draw_block.set(Block::ID_INVALID);
}

void SceneGameplay::OnClose()
{
	m_chathistory_text.clear();
	if (m_minimap)
		m_minimap->markDirty();

	for (auto it : m_cached_textures) {
		m_gui->driver->removeTexture(it.second);
	}
	m_cached_textures.clear();
}


void SceneGameplay::draw()
{
	const auto wsize = m_gui->window_size;
	auto gui = m_gui->guienv;

	PlayerFlags pflags;
	{
		//  Update permission stuff
		auto player = m_gui->getClient()->getMyPlayer();
		pflags = player->getFlags();

		m_may_drag_draw = pflags.check(PlayerFlags::PF_EDIT_DRAW)
			|| pflags.check(PlayerFlags::PF_TMP_EDIT_DRAW);
	}

	{
		SIZEW = wsize.Width * 0.7f;

		m_draw_area = core::recti(
			core::vector2di(1, 1),
			core::dimension2di(SIZEW - 5, wsize.Height - 45)
		);
	}

	// Bottom row
	int x_pos = 5;
	int y_pos = wsize.Height - 35;

	{
		core::recti rect_1(
			core::vector2di(x_pos, y_pos),
			core::dimension2du(40, 30)
		);
		auto eb = gui->addButton(rect_1, nullptr, ID_BtnBack);

		eb->setImage(m_gui->driver->getTexture("assets/textures/icon_leave.png"));
		eb->setScaleImage(true);
		eb->setUseAlphaChannel(true);
	}

	x_pos += 60;

	{
		// God mode toggle button
		core::recti rect_3(
			core::vector2di(x_pos, y_pos),
			core::dimension2du(35, 30)
		);

		auto e = gui->addButton(rect_3, nullptr, ID_BtnGodMode, L"G");
		e->setEnabled(pflags.flags & PlayerFlags::PF_MASK_GODMODE);

		x_pos += 40;
	}

	const int SMILEY_POS = x_pos;
	x_pos += 30 + 10; // Smiley selector

	{
		// Chat box toggle button
		core::recti rect_3(
			core::vector2di(x_pos, y_pos),
			core::dimension2du(35, 30)
		);

		auto eb = gui->addButton(rect_3, nullptr, ID_BtnChat);
		eb->setImage(m_gui->driver->getTexture("assets/textures/icon_chat.png"));
		eb->setScaleImage(true);
		eb->setUseAlphaChannel(true);

		x_pos += 55;

		// Chatbox
		const int width = SIZEW * 0.5f;
		core::recti rect_2(
			core::vector2di((SIZEW - width) / 2, y_pos - 35),
			core::dimension2du(width, 30)
		);

		auto e = gui->addEditBox(
			L"", rect_2, true, nullptr, ID_BoxChat);
		e->setEnabled(!pflags.check(PlayerFlags::PF_TMP_MUTED));
		e->setVisible(false);
	}
;
	const int BLOCKSELECTOR_WIDTH = (8 + 1) * 30 + 5;
	const int BLOCKSELECTOR_POS = std::max(x_pos, (SIZEW - BLOCKSELECTOR_WIDTH) / 2);

	{
		// Minimap toggle button
		core::recti rect_3(
			core::vector2di(SIZEW - 40, y_pos),
			core::dimension2du(30, 30)
		);

		auto eb = gui->addButton(rect_3, nullptr, ID_BtnMinimap);
		eb->setImage(m_gui->driver->getTexture("assets/textures/icon_minimap.png"));
		eb->setScaleImage(true);
		eb->setUseAlphaChannel(true);
	}

	{
		core::recti rect_ch(
			core::vector2di(SIZEW, 160),
			core::dimension2di(wsize.Width - SIZEW, wsize.Height - 5 - 160)
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

	{
		if (!m_world_render)
			m_world_render = new SceneWorldRender(this, m_gui);

		m_world_render->draw();
	}

	m_dirty_world = true; // refresh coins and whatever else

	m_dirty_playerlist = true;

	{
		// Minimap (below block selector)
		if (!m_minimap)
			m_minimap = new SceneMinimap(this, m_gui);

		m_minimap->draw();
	}

	{
		if (!m_smileyselector)
			m_smileyselector = new SceneSmileySelector(m_gui);

		m_smileyselector->setSelectorPos(
			core::vector2di(SMILEY_POS, y_pos)
		);
		m_smileyselector->draw();
	}

	{
		// Block selector GUI
		if (!m_blockselector)
			m_blockselector = new SceneBlockSelector(this, gui);

		m_blockselector->setHotbarPos(
			core::vector2di(BLOCKSELECTOR_POS, y_pos)
		);

		m_blockselector->setEnabled(pflags.flags & PlayerFlags::PF_MASK_EDIT);
		m_blockselector->draw();
	}
}

void SceneGameplay::step(float dtime)
{
	m_world_render->step(dtime);

	updatePlayerlist();
	updateWorldStuff();
	m_minimap->step(dtime);

	if (m_chathistory_text_dirty) {
		m_chathistory_text_dirty = false;
		// GUI elements must be updated in sync with the render thread to avoid segfaults
		m_chathistory->setText(m_chathistory_text.c_str());
	}

	do {
		auto player = m_gui->getClient()->getMyPlayer();
		if (!player)
			break;

		if (player->coins > 0) {
			// Coins text
			core::recti rect(
				core::vector2di(SIZEW - 120, 10),
				core::dimension2di(90, 20)
			);

			wchar_t buf[100];
			swprintf(buf, 100, L"Coins: %d / %d", (int)player->coins, m_total_coins);
			m_gui->font->draw(buf, rect, 0xFFFFFF00, true);
		}
	} while (false);

}

static bool editbox_move_to_end(gui::IGUIEnvironment *guienv, wchar_t charval = L'\0')
{
	auto root = guienv->getRootGUIElement();
	auto element = root->getElementFromId(ID_BoxChat);
	if (!element)
		return false;

	element->setVisible(true);
	root->bringToFront(element);

	SEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.EventType = EET_KEY_INPUT_EVENT;
	ev.KeyInput.PressedDown = true;
	ev.KeyInput.Char = charval;
	ev.KeyInput.Key = KEY_END;
	element->OnEvent(ev);

	guienv->setFocus(element);
	return true;
}

bool SceneGameplay::OnEvent(const SEvent &e)
{
	//if (e.EventType == EET_GUI_EVENT)
	//	printf("event %d, %s\n", e.GUIEvent.EventType, e.GUIEvent.Caller->getTypeName());

	if (m_blockselector && m_blockselector->OnEvent(e))
		return true;
	if (m_smileyselector && m_smileyselector->OnEvent(e))
		return true;

	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				if (e.GUIEvent.Caller->getID() == ID_BtnBack) {
					m_gui->leaveWorld();
					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnGodMode) {
					auto player = m_gui->getClient()->getMyPlayer();
					GameEvent e(GameEvent::G2C_GODMODE);
					e.intval = !player->godmode;
					m_gui->sendNewEvent(e);
					m_gui->guienv->setFocus(nullptr);
					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnChat) {
					auto root = m_gui->guienv->getRootGUIElement();
					auto element = static_cast<gui::IGUIEditBox *>(root->getElementFromId(ID_BoxChat));

					if (!element)
						return false;

					if (!element->isVisible()) {
						// Show
						editbox_move_to_end(m_gui->guienv);
					} else {
						// Hide
						element->setVisible(false);
						m_gui->guienv->setFocus(nullptr);
					}
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
					std::string textn;
					wide_to_utf8(textn, textw);

					if (!m_gamecmd.process(textn)) {
						GameEvent e(GameEvent::G2C_CHAT);
						e.text = new std::string(textn);
						m_gui->sendNewEvent(e);
					}

					if (m_previous_chat_message != textw)
						m_previous_chat_message = textw;

					e.GUIEvent.Caller->setText(L"");
					e.GUIEvent.Caller->setVisible(false);
					m_gui->guienv->setFocus(nullptr);
					return true;
				}
				break;
			default: break;
		}
	}
	if (e.EventType == EET_KEY_INPUT_EVENT) {
		if (e.KeyInput.Key == KEY_LSHIFT || e.KeyInput.Key == KEY_RSHIFT) {
			m_erase_mode = e.KeyInput.PressedDown;
			m_blockselector->setEraseMode(m_erase_mode);
		}
	}
	if (e.EventType == EET_MOUSE_INPUT_EVENT) {

		auto emie = e.MouseInput.Event;
		if (emie == EMIE_LMOUSE_LEFT_UP || emie == EMIE_RMOUSE_LEFT_UP)
			m_drag_draw_block.set(Block::ID_INVALID);

		{
			// Pass through other events
			auto root = m_gui->guienv->getRootGUIElement();
			auto element = root->getElementFromPoint({e.MouseInput.X, e.MouseInput.Y});
			if (element && element != root) {
				return false;
			}
		}

		switch (emie) {
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
					if (!((m_may_drag_draw && m_drag_draw_block.getId() != Block::ID_INVALID) || l_pressed || r_pressed))
						break;

					blockpos_t bp;
					if (!getBlockFromPixel(e.MouseInput.X, e.MouseInput.Y, bp))
						break;

					bool guess_layer = false;
					if (l_pressed) {
						m_blockselector->getBlockUpdate(m_drag_draw_block);
						if (m_drag_draw_block.getId() == 0)
							guess_layer = true;
						else {
							Block bt;
							world->getBlock(bp, &bt);
							if (bt.id == Block::ID_SPIKES)
								m_drag_draw_block.params.param_u8 = (bt.tile + 1) % 4;
						}
					}

					if (r_pressed || guess_layer) {
						// Update block ID on click
						Block bt;
						if (!world->getBlock(bp, &bt))
							break;

						// Pick background if there is no foreground
						m_drag_draw_block.setErase(bt.id == 0);
					}

					BlockUpdate bu = m_drag_draw_block;
					bu.pos = bp;
					if (m_erase_mode)
						bu.setErase(m_drag_draw_block.isBackground());

					if (!m_may_drag_draw)
						m_drag_draw_block.set(Block::ID_INVALID);

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

					m_world_render->getCameraPos()->Z *= (1 - dir * 0.1);
				}
				break;
			default: break;
		}
	}
	if (e.EventType == EET_KEY_INPUT_EVENT) {
		auto focused = m_gui->guienv->getFocus();
		if (focused) {
			if (focused->getID() == ID_BoxChat) {
				if (handleChatInput(e))
					return true;
			}

			if (focused->getType() == gui::EGUIET_EDIT_BOX) {
				// Skip other inputs if an edit box is selected
				return false;
			}
		}

		if (handleChatInput(e))
			return true;

		auto player = m_gui->getClient()->getMyPlayer();
		if (!player)
			return false;

		EKEY_CODE keycode = e.KeyInput.Key;
		bool down = e.KeyInput.PressedDown;
		auto controls = player->getControls();

		// The Client performs physics of all players, including ours.
		switch (keycode) {
			case KEY_KEY_M:
				if (down) {
					if (m_minimap)
						m_minimap->toggleVisibility();
					return true;
				}
				break;
			case KEY_KEY_G:
				if (down) {
					GameEvent e(GameEvent::G2C_GODMODE);
					e.intval = !player->godmode;
					m_gui->sendNewEvent(e);
					return true;
				}
				break;
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
		case E::C2G_META_UPDATE:
			m_dirty_playerlist = true;
			break;
		case E::C2G_MAP_UPDATE:
			m_world_render->markDirty();
			m_dirty_world = true;

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
		case E::C2G_LOCAL_CHAT:
			{
				const char *who = "* SERVER";
				if (e.type_c2g == E::C2G_LOCAL_CHAT)
					who = "* LOCAL";
				if (e.player_chat->player)
					who = e.player_chat->player->name.c_str();

				char buf[200];
				snprintf(buf, sizeof(buf), "%s: %s\n",
					who, e.player_chat->message.c_str()
				);

				std::wstring line;
				utf8_to_wide(line, buf);
				m_chathistory_text.append(line.c_str());
				m_chathistory_text_dirty = true;
			}
			return true;
		case E::C2G_PLAYERFLAGS:
			m_gui->requestRenew();
			return true;
		default: break;
	}
	return false;
}

bool SceneGameplay::handleChatInput(const SEvent &e)
{
	// dynamic_cast does not work and I don't know why
	auto root = m_gui->guienv->getRootGUIElement();
	auto element = static_cast<gui::IGUIEditBox *>(root->getElementFromId(ID_BoxChat));

	if (!element)
		return false;

	bool focused = element == m_gui->guienv->getFocus();
	EKEY_CODE key = e.KeyInput.Key;
	bool down = e.KeyInput.PressedDown;

	if (key == KEY_ESCAPE && focused) {
		element->setText(L"");
		element->setVisible(false);
		m_gui->guienv->setFocus(nullptr);
		return true;
	}

	if (key == KEY_UP && focused) {
		if (!m_previous_chat_message.empty()) {
			element->setText(m_previous_chat_message.c_str());
			return true;
		}
	}

	if (key == KEY_DOWN && focused) {
		element->setText(L"");
		return true;
	}

	if (key == KEY_TAB && focused) {
		if (down)
			return true; // eat it

		// Nickname autocompletion
		std::wstring text(element->getText());
		if (text.empty())
			return true;

		size_t word_start = 0;
		for (size_t i = 1; i < text.size(); ++i) {
			if (std::isspace(text[i - 1]) && !std::isspace(text[i]))
				word_start = i;
		}

		std::string last_word;
		wide_to_utf8(last_word, &text[word_start]);
		for (char &c : last_word)
			c = toupper(c);

		auto players = m_gui->getClient()->getPlayerList();
		std::string playername;
		for (auto p : *players.ptr()) {
			if (p.second->name.rfind(last_word, 0) == 0) {
				if (!playername.empty())
					return false; // Ambigious

				playername = p.second->name;
			}
		}
		if (playername.empty())
			return true;

		std::wstring namew;
		utf8_to_wide(namew, playername.c_str());
		text.resize(word_start);
		text.append(namew);
		text.append(L" ");

		element->setText(text.c_str());
		editbox_move_to_end(m_gui->guienv);
		return true;
	}

	if (!focused) {
		if (e.KeyInput.Char == L'/' || (key == KEY_KEY_T && !down)) {
			// Focus chat window
			if (editbox_move_to_end(m_gui->guienv, e.KeyInput.Char))
				return true;
		}
		if (key == KEY_RETURN && down) {
			// Focus chat window
			if (editbox_move_to_end(m_gui->guienv, L'\0'))
				return true;
		}
	}
	return false;
}


bool SceneGameplay::getBlockFromPixel(int x, int y, blockpos_t &bp)
{
	core::vector2di mousepos(x, y);

	if (!m_draw_area.isPointInside(mousepos))
		return false;

	auto shootline = m_world_render->getShootLine(mousepos);
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

video::ITexture *SceneGameplay::generateTexture(const std::string &text, u32 color, u32 bgcolor)
{
	auto it = m_cached_textures.find(text);
	if (it != m_cached_textures.end())
		return it->second;

	core::stringw textw;
	core::multibyteToWString(textw, text.c_str());

	auto driver = m_gui->driver;
	auto dim = m_gui->font->getDimension(textw.c_str());
	dim.Width += 2;

	auto texture = driver->addRenderTargetTexture(dim); //, "rt", video::ECF_A8R8G8B8);
	driver->setRenderTarget(texture, true, true, video::SColor(bgcolor));

	m_gui->font->draw(textw.c_str(), core::recti(core::vector2di(2,0), dim), 0xFF555555); // Shadow
	m_gui->font->draw(textw.c_str(), core::recti(core::vector2di(1,-1), dim), color);

	driver->setRenderTarget(nullptr, video::ECBF_ALL);

	m_cached_textures.emplace(text, texture);
	return texture;
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

	if (playerlist) {
		root->removeChild(playerlist);
		root->removeChild(root->getElementFromId(ID_LabelTitle));
	}

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
		core::recti rect_text(
			core::vector2di(SIZEW, 5),
			core::dimension2du(wsize.Width - SIZEW, 45)
		);
		const auto &meta = world->getMeta();
		std::string src_text;
		if (!meta.title.empty())
			src_text.append(meta.title);
		else
			src_text.append("(Untitled)");
		src_text.append("\r\nID: " + meta.id);
		src_text.append(" | Owner: " + meta.owner);

		core::stringw dst_text;
		core::multibyteToWString(dst_text, src_text.c_str());

		auto e = gui->addStaticText(dst_text.c_str(), rect_text);
		e->setID(ID_LabelTitle);
		e->setOverrideColor(Gui::COLOR_ON_BG);
	}
}

void SceneGameplay::updateWorldStuff()
{
	if (!m_dirty_world)
		return;
	m_dirty_world = false;

	auto world = m_gui->getClient()->getWorld();
	if (!world)
		return;

	// Update coin count
	auto blocks = world->getBlocks(Block::ID_COIN, nullptr);
	m_total_coins = blocks.size();
}
