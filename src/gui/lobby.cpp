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
	ID_BoxWorldID,
	ID_BtnJoin,
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

	{
		auto tab = tc->addTab(L"Public worlds");
		tab->setBackgroundColor(tc_bgcolor);
		tab->setDrawBackground(true);
		auto rect = m_gui->getRect({2, 2}, {76, 40});
		m_publiclist = gui->addListBox(rect, tab, ID_ListPublic, true);
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
		auto rect = m_gui->getRect({2, 2}, {76, 40});
		m_mylist = gui->addListBox(rect, tab, ID_ListMine, true);
	}

	{
		// Custom world ID box
		auto rect_lab = m_gui->getRect({30, 70}, {10, -30});
		auto rect_box = m_gui->getRect({0, 0}, {15, -30});
		rect_box += rect_lab.LowerRightCorner + core::vector2di(0, -35);

		{
			auto e = gui->addStaticText(L"Title", rect_lab);
			e->setOverrideColor(Gui::COLOR_ON_BG);

			std::wstring tmp;
			utf8_to_wide(tmp, title.c_str());
			gui->addEditBox(tmp.c_str(), rect_box, true, nullptr, ID_BoxTitle);
		}

		rect_lab += core::vector2di(0, 35);
		rect_box += core::vector2di(0, 35);
		{
			auto e = gui->addStaticText(L"Code", rect_lab);
			e->setOverrideColor(Gui::COLOR_ON_BG);

			std::wstring tmp;
			utf8_to_wide(tmp, code.c_str());
			gui->addEditBox(tmp.c_str(), rect_box, true, nullptr, ID_BoxCode);
		}

		rect_lab += core::vector2di(0, 35);
		rect_box += core::vector2di(0, 35);
		{
			auto e = gui->addStaticText(L"Type", rect_lab);
			e->setOverrideColor(Gui::COLOR_ON_BG);

			auto c = gui->addComboBox(rect_box, nullptr, ID_SelMode);
			c->addItem(L"Temporary",   (u32)WorldMeta::Type::TmpSimple);
			c->addItem(L"Temp + Draw", (u32)WorldMeta::Type::TmpDraw);
			c->addItem(L"Persistent",  (u32)WorldMeta::Type::Persistent);
		}

		rect_box += core::vector2di(0, 35);
		gui->addButton(rect_box, nullptr, ID_BtnCreate, L"Create");
	}

	{
		// Custom world ID box
		auto rect = m_gui->getRect({65, 80}, {15, -30});
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

					GameEvent e(GameEvent::G2C_CREATE_WORLD);
					e.wc_data = new GameEvent::WorldCreationData {
						.mode = s_mode->getSelected(),
						.title = title,
						.code = code
					};
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
				break;
			case gui::EGET_LISTBOX_SELECTED_AGAIN:
				if (e.GUIEvent.Caller->getID() == ID_ListPublic) {
					gui::IGUIListBox *list = (gui::IGUIListBox *)e.GUIEvent.Caller;

					try {
						world_id = m_public_index_to_worldid.at(list->getSelected());
					} catch (std::exception &) {
						break;
					}

					m_gui->joinWorld(this);
				}
				if (e.GUIEvent.Caller->getID() == ID_ListMine) {
					gui::IGUIListBox *list = (gui::IGUIListBox *)e.GUIEvent.Caller;

					try {
						world_id = m_my_index_to_worldid.at(list->getSelected());
					} catch (std::exception &) {
						break;
					}

					m_gui->joinWorld(this);
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
	m_public_index_to_worldid.clear();
	m_my_index_to_worldid.clear();

	auto player = m_gui->getClient()->getMyPlayer();
	auto worlds = m_gui->getClient()->world_list;

	for (const auto &it : worlds) {
		bool is_mine = player->name == it.second.owner;
		auto size = it.second.size;

		std::ostringstream os;
		os << "[" << it.second.online << " online] ";
		if (!it.second.title.empty())
			os << it.second.title;
		else
			os << "(Untitled)";

		os << " (id=" << it.first;
		os << ", " << size.X << "x" << size.Y << " )";
		if (is_mine)
			os << (it.second.is_public ? " - public" : " - private");
		else
			os << " by " << it.second.owner;

		core::stringw textw;
		core::multibyteToWString(textw, os.str().c_str());
		auto dst = is_mine ? m_mylist : m_publiclist;

		dst->addItem(textw.c_str());

		if (is_mine)
			m_my_index_to_worldid.push_back(it.first);
		else
			m_public_index_to_worldid.push_back(it.first);
	}

	m_refreshbtn->setEnabled(true);
}

