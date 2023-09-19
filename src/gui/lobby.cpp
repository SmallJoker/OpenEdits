#include "lobby.h"
#include "core/blockmanager.h" // populateTextures
#include "client/client.h"
#include "client/localplayer.h"
#include <IGUIButton.h>
#include <IGUIComboBox.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIListBox.h>
#include <IGUIStaticText.h>
#include <IGUITabControl.h>
#include <IVideoDriver.h>
#include <sstream>

enum ElementId : int {
	ID_BtnRefresh,
	ID_ListPublic,
	ID_ListMine,
	ID_ListImport,
	ID_BoxWorldID,
	ID_BtnJoin,
	ID_BtnChangePass,
	ID_BtnDisconnect,
	// World creation:
	ID_BoxTitle,
	ID_BoxCode,
	ID_SelMode,
	ID_BtnCreate
};


SceneLobby::SceneLobby()
{
}

void SceneLobby::OnOpen()
{
	title = generate_world_title();
	code.clear();

	{
		// Update world listing
		GameEvent e(GameEvent::G2C_LOBBY_REQUEST);
		m_gui->sendNewEvent(e);
	}
}

void SceneLobby::draw()
{
	// only on demand
	g_blockmanager->populateTextures();


	core::recti rect_title(
		core::vector2di(m_gui->window_size.Width * 0.45f, 20),
		core::dimension2di(100, 30)
	);

	auto gui = m_gui->guienv;

	auto text = gui->addStaticText(L"Lobby", rect_title);
	text->setOverrideColor(Gui::COLOR_ON_BG);

	auto rect_tc =  m_gui->getRect({10, 10}, {80, 50});
	auto tc = gui->addTabControl(rect_tc);
	video::SColor tc_bgcolor(m_gui->guienv->getSkin()->getColor(gui::EGDC_3D_FACE));

	auto list_rect = m_gui->getRect({2, 2}, {76, 40});
	{
		auto tab = tc->addTab(L"Public worlds");
		tab->setBackgroundColor(tc_bgcolor);
		tab->setDrawBackground(true);
		m_publiclist = gui->addListBox(list_rect, tab, ID_ListPublic, true);
	}

	{
		core::recti rect_btn(
			core::vector2di(rect_tc.LowerRightCorner.X - 100, rect_tc.UpperLeftCorner.Y - 10),
			core::dimension2di(100, 30)
		);
		m_refreshbtn = gui->addButton(rect_btn, nullptr, ID_BtnRefresh, L"Refresh");
	}

	{
		auto tab = tc->addTab(L"My worlds");
		tab->setBackgroundColor(tc_bgcolor);
		tab->setDrawBackground(true);
		m_mylist = gui->addListBox(list_rect, tab, ID_ListMine, true);
	}

	{
		auto tab = tc->addTab(L"Importable worlds");
		tab->setBackgroundColor(tc_bgcolor);
		tab->setDrawBackground(true);
		m_importlist = gui->addListBox(list_rect, tab, ID_ListImport, true);
	}

	{
		// New world creation options
		const core::vector2di VSPACING(0, 35);

		auto rect_lab = m_gui->getRect({25, 65}, {10, -30});
		auto rect_box = m_gui->getRect({0, 0}, {17, -30}) + core::vector2di(0, -35);
		auto get_box_rect = [&] () {
			return rect_box + rect_lab.LowerRightCorner;
		};

		{
			auto e = gui->addStaticText(L"Title", rect_lab);
			e->setOverrideColor(Gui::COLOR_ON_BG);

			std::wstring tmp;
			utf8_to_wide(tmp, title.c_str());
			gui->addEditBox(tmp.c_str(), get_box_rect(), true, nullptr, ID_BoxTitle);
			rect_lab += VSPACING;
		}

		{
			auto e = gui->addStaticText(L"Edit code", rect_lab);
			e->setOverrideColor(Gui::COLOR_ON_BG);

			std::wstring tmp;
			utf8_to_wide(tmp, code.c_str());
			gui->addEditBox(tmp.c_str(), get_box_rect(), true, nullptr, ID_BoxCode);
			rect_lab += VSPACING;
		}

		{
			auto e = gui->addStaticText(L"Type", rect_lab);
			e->setOverrideColor(Gui::COLOR_ON_BG);

			auto c = gui->addComboBox(get_box_rect(), nullptr, ID_SelMode);
			c->addItem(L"Temporary",   (u32)WorldMeta::Type::TmpSimple);
			c->addItem(L"Temp + Draw", (u32)WorldMeta::Type::TmpDraw);
			c->addItem(L"Persistent",  (u32)WorldMeta::Type::Persistent);
			rect_lab += VSPACING;
		}

		gui->addButton(get_box_rect(), nullptr, ID_BtnCreate, L"Create");
	}

	{
		// Custom world ID box
		auto rect = m_gui->getRect({60, 70}, {15, -30});
		gui->addEditBox(L"", rect, true, nullptr, ID_BoxWorldID);

		core::recti rect_btn(
			rect.LowerRightCorner + core::vector2di(10, -30),
			core::dimension2di(60, 30)
		);
		gui->addButton(rect_btn, nullptr, ID_BtnJoin, L"Join");

		core::recti rect_lab(
			rect.UpperLeftCorner + core::vector2di(0, -25),
			core::dimension2di(200, 25)
		);
		auto e = gui->addStaticText(L"Join world by ID", rect_lab);
		e->setOverrideColor(Gui::COLOR_ON_BG);

	}

	// Exit server
	{
		auto rect_btn =  m_gui->getRect({10, 88}, {-40, -30});

		auto eb = gui->addButton(rect_btn, nullptr, ID_BtnDisconnect);
		eb->setImage(m_gui->driver->getTexture("assets/textures/icon_leave.png"));
		eb->setScaleImage(true);
		eb->setUseAlphaChannel(true);
	}

	// Change password
	{
		auto rect_btn =  m_gui->getRect({-10, 88}, {-150, -30});
		gui->addButton(rect_btn, nullptr, ID_BtnChangePass, L"Change password");
	}

	m_dirty_worldlist = true;
	updateWorldList();
}

void SceneLobby::step(float dtime)
{
	updateWorldList();
}

bool SceneLobby::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
			case gui::EGET_EDITBOX_ENTER:
				if (e.GUIEvent.Caller->getID() == ID_BtnJoin
						|| e.GUIEvent.Caller->getID() == ID_BoxWorldID) {
					auto root = m_gui->guienv->getRootGUIElement();
					auto editbox = root->getElementFromId(ID_BoxWorldID);

					if (!editbox->getText()[0])
						return true; // Empty box

					wide_to_utf8(world_id, editbox->getText());
					m_gui->joinWorld(this);
					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnCreate) {
					auto root = m_gui->guienv->getRootGUIElement();
					auto b_title = root->getElementFromId(ID_BoxTitle);
					auto b_code = root->getElementFromId(ID_BoxCode);
					auto s_mode = (gui::IGUIComboBox *)root->getElementFromId(ID_SelMode);

					wide_to_utf8(title, b_title->getText());
					wide_to_utf8(code, b_code->getText());

					auto wc_data = new GameEvent::WorldCreationData();
					wc_data->mode = s_mode->getSelected();
					wc_data->title = title;
					wc_data->code = code;

					GameEvent e(GameEvent::G2C_CREATE_WORLD);
					e.wc_data = wc_data;
					m_gui->sendNewEvent(e);
					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnRefresh) {
					e.GUIEvent.Caller->setEnabled(false);
					GameEvent e(GameEvent::G2C_LOBBY_REQUEST);
					m_gui->sendNewEvent(e);
					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnDisconnect) {
					m_gui->disconnect();
					return true;
				}
				if (e.GUIEvent.Caller->getID() == ID_BtnChangePass) {
					e.GUIEvent.Caller->setEnabled(false);
					m_gui->setSceneLoggedIn(SceneHandlerType::Register);
					return true;
				}
				break;
			case gui::EGET_LISTBOX_SELECTED_AGAIN:
				{
					std::vector<std::string> *lookup = nullptr;
					switch (e.GUIEvent.Caller->getID()) {
						case ID_ListPublic: lookup = &m_public_index_to_worldid; break;
						case ID_ListMine:   lookup = &m_my_index_to_worldid;     break;
						case ID_ListImport: lookup = &m_import_index_to_worldid; break;
					}
					if (lookup) {
						gui::IGUIListBox *list = (gui::IGUIListBox *)e.GUIEvent.Caller;

						try {
							world_id = lookup->at(list->getSelected());
						} catch (std::exception &) {
							break;
						}

						m_gui->joinWorld(this);
					}
				}
				break;
			default: break;
		}
	}
	return false;
}

bool SceneLobby::OnEvent(GameEvent &e)
{
	using E = GameEvent::C2G_Enum;

	switch (e.type_c2g) {
		case E::C2G_LOBBY_UPDATE:
			m_dirty_worldlist = true;
			return true;
		default: break;
	}
	return false;
}

void SceneLobby::updateWorldList()
{
	if (!m_dirty_worldlist)
		return;
	m_dirty_worldlist = false;

	m_publiclist->clear();
	m_mylist->clear();
	m_importlist->clear();
	m_public_index_to_worldid.clear();
	m_my_index_to_worldid.clear();
	m_import_index_to_worldid.clear();

	auto player = m_gui->getClient()->getMyPlayer();
	auto worlds = m_gui->getClient()->world_list;

	for (const auto &it : worlds) {
		bool is_mine = player->name == it.owner;
		auto size = it.size;

		std::ostringstream os;
		os << "[" << it.online << " online] ";
		if (!it.title.empty())
			os << it.title;
		else
			os << "(Untitled)";

		os << " (id=" << it.id;
		os << ", " << size.X << "x" << size.Y << " )";
		if (is_mine)
			os << (it.is_public ? " - public" : " - private");
		else if (!it.owner.empty())
			os << " by " << it.owner;

		core::stringw textw;
		core::multibyteToWString(textw, os.str().c_str());
		if (is_mine) {
			m_mylist->addItem(textw.c_str());
			m_my_index_to_worldid.push_back(it.id);
		}

		// Importable worlds
		if (WorldMeta::idToType(it.id) == WorldMeta::Type::Readonly) {
			m_importlist->addItem(textw.c_str());
			m_import_index_to_worldid.push_back(it.id);
		}

		// Public worlds (or player-specific private worlds)
		if (it.online > 0) {
			m_publiclist->addItem(textw.c_str());
			m_public_index_to_worldid.push_back(it.id);
		}
	}

	m_refreshbtn->setEnabled(true);
}

