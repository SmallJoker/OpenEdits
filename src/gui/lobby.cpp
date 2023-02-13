#include "lobby.h"
#include "client/client.h"
#include <IGUIButton.h>
#include <IGUIEnvironment.h>
#include <IGUIListBox.h>
#include <IGUIStaticText.h>
#include <sstream>

enum ElementId : int {
	ID_LabelLobby = 100,
	ID_BtnRefresh,
	ID_ListWorlds,
	ID_BoxWorldID,
	ID_BtnJoin,
};


SceneLobby::SceneLobby()
{
}

void SceneLobby::draw()
{
	core::recti rect_title(
		core::vector2di(50, 20),
		core::dimension2di(100, 30)
	);

	const auto wsize = m_gui->window_size;

	auto text = m_gui->gui->addStaticText(L"Lobby", rect_title);
	text->setOverrideColor(0xFFFFFFFF);

	{
		core::recti rect_list(
			core::vector2di(50, 60),
			core::dimension2di(wsize.Width - 50 * 2, wsize.Height - 200)
		);
		m_worldlist = m_gui->gui->addListBox(rect_list, nullptr, ID_ListWorlds, false);

		core::recti rect_btn(
			rect_list.UpperLeftCorner,
			core::dimension2di(100, 30)
		);
		rect_btn += core::vector2di(wsize.Width - 200, -40);
		m_refreshbtn = m_gui->gui->addButton(rect_btn, nullptr, ID_BtnRefresh, L"Refresh");
	}

	{
		// Custom world ID box
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
				if (e.GUIEvent.Caller->getID() == ID_BtnJoin) {
					auto root = m_gui->gui->getRootGUIElement();
					auto editbox = root->getElementFromId(ID_BoxWorldID);

					wStringToMultibyte(world_id, editbox->getText());
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
				if (e.GUIEvent.Caller->getID() == ID_ListWorlds) {
					gui::IGUIListBox *list = (gui::IGUIListBox *)e.GUIEvent.Caller;

					try {
						world_id = m_index_to_worldid.at(list->getSelected());
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

	m_worldlist->clear();
	m_index_to_worldid.clear();

	auto worlds = m_gui->getClient()->world_list;

	for (const auto &it : worlds) {
		auto size = it.second.size;

		std::ostringstream os;
		os << "[" << it.second.online << " online]";
		os << " id=" << it.first;
		os << " ( " << size.X << "x" << size.Y << " )";
		os << " by " << it.second.owner;

		core::stringw textw;
		core::multibyteToWString(textw, os.str().c_str());
		auto i = m_worldlist->addItem(textw.c_str());
		m_worldlist->setItemOverrideColor(i, 0xFFFFFFFF);

		m_index_to_worldid.push_back(it.first);
	}

	m_refreshbtn->setEnabled(true);
}

