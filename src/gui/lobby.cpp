#include "lobby.h"
#include "core/blockmanager.h" // populateTextures
#include "client/client.h"
#include "client/localplayer.h"
#include <IGUIButton.h>
#include <IGUIEnvironment.h>
#include <IGUIListBox.h>
#include <IGUIStaticText.h>
#include <sstream>

enum ElementId : int {
	ID_LabelLobby = 100,
	ID_BtnRefresh,
	ID_ListPublic,
	ID_ListMine,
	ID_BoxWorldID,
	ID_BtnJoin,
};


SceneLobby::SceneLobby()
{
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

	{
		auto rect = m_gui->getRect({10, 15}, {80, 30});
		m_publiclist = gui->addListBox(rect, nullptr, ID_ListPublic, true);

		core::recti rect_lab(
			rect.UpperLeftCorner + core::vector2di(0, -25),
			core::dimension2di(100, 25)
		);
		auto e = gui->addStaticText(L"Public worlds", rect_lab);
		e->setOverrideColor(Gui::COLOR_ON_BG);

		core::recti rect_btn(
			core::vector2di(rect.LowerRightCorner.X - 100, rect.UpperLeftCorner.Y - 40),
			core::dimension2di(100, 30)
		);
		m_refreshbtn = gui->addButton(rect_btn, nullptr, ID_BtnRefresh, L"Refresh");
	}

	{
		auto rect = m_gui->getRect({10, 51}, {80, 25});
		m_mylist = gui->addListBox(rect, nullptr, ID_ListMine, true);

		core::recti rect_lab(
			rect.UpperLeftCorner + core::vector2di(0, -25),
			core::dimension2di(100, 25)
		);
		auto e = gui->addStaticText(L"My worlds", rect_lab);
		e->setOverrideColor(Gui::COLOR_ON_BG);
	}

	{
		// Custom world ID box
		auto rect = m_gui->getRect({10, 83}, {20, -30});
		gui->addEditBox(L"", rect, true, nullptr, ID_BoxWorldID);

		core::recti rect_lab(
			rect.UpperLeftCorner + core::vector2di(0, -25),
			core::dimension2di(300, 25)
		);
		auto e = gui->addStaticText(L"Custom world ID", rect_lab);
		e->setOverrideColor(Gui::COLOR_ON_BG);

		auto rect_btn =  m_gui->getRect({32, 83}, {-100, -30});
		gui->addButton(rect_btn, nullptr, ID_BtnJoin, L"Join");
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
				if (e.GUIEvent.Caller->getID() == ID_BtnRefresh) {
					e.GUIEvent.Caller->setEnabled(false);
					GameEvent e(GameEvent::G2C_LOBBY_REQUEST);
					m_gui->sendNewEvent(e);
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
		os << "[" << it.second.online << " online]";
		os << " id=" << it.first;
		os << " ( " << size.X << "x" << size.Y << " )";
		if (is_mine)
			os << (it.second.is_public ? " - public" : " - private");
		else
			os << " by " << it.second.owner;

		core::stringw textw;
		core::multibyteToWString(textw, os.str().c_str());
		auto dst = is_mine ? m_mylist : m_publiclist;

		auto i = dst->addItem(textw.c_str());
		dst->setItemOverrideColor(i, 0xFFFFFFFF);

		if (is_mine)
			m_my_index_to_worldid.push_back(it.first);
		else
			m_public_index_to_worldid.push_back(it.first);
	}

	m_refreshbtn->setEnabled(true);
}

