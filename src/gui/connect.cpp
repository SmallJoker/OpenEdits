#include "connect.h"
#include "guilayout/guilayout_irrlicht.h"
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIImage.h>
#include <IGUIListBox.h>
#include <IGUIStaticText.h>
#include <IVideoDriver.h>
#include <fstream>
#include <vector2d.h>

enum ElementId : int {
	ID_BoxNickname = 101,
	ID_BoxPassword,
	ID_BoxAddress,
	ID_BtnConnect,
	ID_BtnHost,
	ID_ListServers,
	ID_BtnDelServer
};

SceneConnect::SceneConnect() :
	SceneHandler(L"Connect")
{

}

// -------------- Public members -------------

static guilayout::Table layout_root;
void SceneConnect::OnClose()
{
	layout_root.clear();
	if (record_login) {
		record_login = false;

		bool contains = false;

		std::string address_a, nickname_a;
		wide_to_utf8(address_a, address.c_str());
		wide_to_utf8(nickname_a, nickname.c_str());

		for (char &c : address_a)
			c = tolower(c);
		for (char &c : nickname_a)
			c = toupper(c);

		if (nickname_a.rfind("GUEST", 0) == 0)
			goto end;

		std::ifstream is("client_servers.txt");
		std::string line;
		while (std::getline(is, line)) {
			LoginInfo info;
			std::string address_f = get_next_part(line);
			std::string nickname_f =  get_next_part(line);
			if (address_f == address_a && nickname_f == nickname_a)
				goto end;
		}
		is.close();

		if (!contains) {
			std::ofstream os("client_servers.txt", std::ios_base::app);
			os << address_a << " " << nickname_a << std::endl;
			os.close();
		}
	}

end:
	return;
}

static u16 BUTTON_H = 20;

static void set_text_props(guilayout::IGUIElementWrapper *w)
{
	w->expand = { 0, 0 };
	w->margin = { 1, 1, 1, 0 };
	w->min_size = { 100, BUTTON_H }; // for Y center
}

static void set_field_props(guilayout::IGUIElementWrapper *w)
{
	w->expand = { 5, 1 };
	w->margin = { 1, 1, 1, 1 };
	w->min_size = { 100, (u16)(BUTTON_H * 1.5) }; // for Y center
}

void SceneConnect::draw()
{
	using namespace guilayout;
	using WRAP = guilayout::IGUIElementWrapper;

	Table &root = layout_root;
	root.clear();
	root.setSize(3, 5);
	root.col(0)->weight = 20;
	root.col(2)->weight = 20;
	root.row(4)->weight = 20;

	auto gui = m_gui->guienv;
	core::recti norect;
	core::vector2di nopos;

	{
		// Logo
		auto texture = gui->getVideoDriver()->getTexture("assets/logo.png");
		auto dim = texture->getOriginalSize();

		auto *i_img = gui->addImage(texture, nopos, false, 0, -1, L"test test");
		auto *g_img = root.add<WRAP>(1, 1, i_img);
		g_img->expand = { 0, 0 };
		g_img->margin = { 1, 1, 1, 1 };
		g_img->min_size = { (u16)dim.Width, (u16)dim.Height };
		//ge->fixed_aspect_ratio = true;
	}

	Table *table_prompt = root.add<Table>(1, 2);
	table_prompt->setSize(3, 3);
	table_prompt->col(1)->weight = 40;
	table_prompt->col(2)->weight = 20;

	u16 column = 0;

	{
		auto *i_text = gui->addStaticText(L"Username", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		auto *g_text = table_prompt->add<WRAP>(0, column, i_text);

		auto *i_box = gui->addEditBox(
			nickname.c_str(), norect, true, nullptr, ID_BoxNickname);
		auto *g_box = table_prompt->add<WRAP>(1, column, i_box);

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnHost, L"Host");
		auto *g_btn = table_prompt->add<WRAP>(2, column, i_btn);

		set_text_props(g_text);
		set_field_props(g_box);
		set_field_props(g_btn);

		column++;
	}

	{
		auto i_text = gui->addStaticText(L"Password", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		auto *g_text = table_prompt->add<WRAP>(0, column, i_text);

		auto *i_box = gui->addEditBox(
			password.c_str(), norect, true, nullptr, ID_BoxPassword);
		i_box->setPasswordBox(true);
		auto *g_box = table_prompt->add<WRAP>(1, column, i_box);

		set_text_props(g_text);
		set_field_props(g_box);

		column++;
	}

	{
		auto i_text = gui->addStaticText(L"Address", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		auto *g_text = table_prompt->add<WRAP>(0, column, i_text);

		core::stringw str;

		auto *i_box = gui->addEditBox(
			address.c_str(), norect, true, nullptr, ID_BoxAddress);
		auto *g_box = table_prompt->add<WRAP>(1, column, i_box);

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnConnect, L"Connect");
		auto *g_btn = table_prompt->add<WRAP>(2, column, i_btn);

		set_text_props(g_text);
		set_field_props(g_box);
		set_field_props(g_btn);

		column++;
	}

	root.row(3)->weight = 20;
	Table *table_srv = root.add<Table>(1, 3);
	table_srv->setSize(2, 1);
	table_srv->col(1)->weight = 40 + 20; // editbox + button
	table_srv->margin = { 1, 0, 1, 0 };

	{
		FlexBox *box_left = table_srv->add<FlexBox>(0, 0);
		box_left->box_axis = Element::SIZE_Y;
		box_left->allow_wrap = false; // H center

		// Server selector dropdown
		auto i_text = gui->addStaticText(L"Servers", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		auto *g_text = box_left->add<WRAP>(i_text);

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnDelServer, L"Remove");
		auto *g_btn = box_left->add<WRAP>(i_btn);

		auto *i_list = gui->addListBox(norect, nullptr, ID_ListServers, true);
		auto *g_list = table_srv->add<WRAP>(1, 0, i_list);

		set_text_props(g_text);
		g_btn->expand = { 2, 1 };
		g_btn->margin = { 10, 1, 1, 0 };
		g_btn->min_size = { 50, BUTTON_H };

		g_list->margin = { 1, 1, 1, 1 };
		g_list->min_size = { 200, 40 };

		updateServers();
	}

	{
		auto wsize = m_gui->window_size;
		root.start({0, 0}, {(u16)wsize.Width, (u16)wsize.Height});
	}
}

void SceneConnect::step(float dtime)
{
	using namespace guilayout;

	if (0) {
		// For debugging
		layout_root.doRecursive([this] (Element *e) -> bool {
			using Dir = guilayout::Element::Direction;
			if (!e)
				return true;

			this->m_gui->driver->draw2DLine(
				core::vector2di(e->pos[Dir::DIR_LEFT], e->pos[Dir::DIR_UP]),
				core::vector2di(e->pos[Dir::DIR_RIGHT], e->pos[Dir::DIR_DOWN]),
				0xFFFF0000
			);
			return true;
		});
	}
}

bool SceneConnect::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				{
					int id = e.GUIEvent.Caller->getID();
					if (id == ID_BtnConnect || id == ID_BtnHost) {
						onSubmit(id);
						return true;
					}
					if (id == ID_BtnDelServer) {
						auto root = m_gui->guienv->getRootGUIElement();
						auto listbox = (gui::IGUIListBox *)root->getElementFromId(ID_ListServers);

						if (!listbox)
							return false;

						int index;
						try {
							index = listbox->getSelected();
							(void)m_index_to_address.at(index); // valid?
						} catch (std::exception &) {
							break;
						}

						// Remove index from the file
						removeServer(index);
						updateServers();
						return true;
					}
				}
				break;
			case gui::EGET_LISTBOX_CHANGED:
				if (e.GUIEvent.Caller->getID() == ID_ListServers) {
					gui::IGUIListBox *listbox = (gui::IGUIListBox *)e.GUIEvent.Caller;

					try {
						auto info = m_index_to_address.at(listbox->getSelected());
						address = info.address.c_str();
						nickname = info.nickname.c_str();

						m_gui->requestRenew();
					} catch (std::exception &) {
						break;
					}
				}
				break;
			default: break;
		}
	}
	return false;
}

bool SceneConnect::OnEvent(GameEvent &e)
{
	return false;
}


void SceneConnect::onSubmit(int elementid)
{
	auto root = m_gui->guienv->getRootGUIElement();

	nickname = root->getElementFromId(ID_BoxNickname)->getText();
	password = root->getElementFromId(ID_BoxPassword)->getText();
	address = root->getElementFromId(ID_BoxAddress)->getText();

	start_localhost = (elementid == ID_BtnHost);

	m_gui->connect(this);
}

static const std::string SERVERS_FILE = "client_servers.txt";

void SceneConnect::removeServer(int index)
{
	const LoginInfo &ref = m_index_to_address.at(index);

	// Read in the saved servers
	std::ifstream is(SERVERS_FILE);
	std::ofstream os(SERVERS_FILE + ".tmp", std::ios_base::trunc);
	std::string line;
	while (std::getline(is, line)) {
		std::string original_line = line;

		LoginInfo info;
		utf8_to_wide(info.address, get_next_part(line).c_str());
		utf8_to_wide(info.nickname, get_next_part(line).c_str());

		if (info.address == ref.address && info.nickname == ref.nickname) {
			// Skip
			continue;
		}
		os << original_line << std::endl;
	}

	is.close();
	os.close();

	std::remove(SERVERS_FILE.c_str());
	std::rename((SERVERS_FILE + ".tmp").c_str(), SERVERS_FILE.c_str());
}


void SceneConnect::updateServers()
{
	auto root = m_gui->guienv->getRootGUIElement();
	auto listbox = (gui::IGUIListBox *)root->getElementFromId(ID_ListServers);

	if (!listbox)
		return;

	{
		m_index_to_address.clear();

		// Read in the saved servers
		std::ifstream is("client_servers.txt");
		std::string line;
		while (std::getline(is, line)) {
			LoginInfo info;
			utf8_to_wide(info.address, get_next_part(line).c_str());
			utf8_to_wide(info.nickname, get_next_part(line).c_str());
			if (info.nickname.empty())
				continue;

			m_index_to_address.push_back(info);
		}
	}

	// Add to the gui
	listbox->clear();
	for (auto &info : m_index_to_address) {
		std::wstring label = info.nickname + L"  -  " + info.address;

		auto i = listbox->addItem(label.c_str());
		// TODO: "Invalid read of size 32" while doing string compare. Why?
		if (info.address == address.c_str() && info.nickname == nickname.c_str())
			listbox->setSelected(i);
	}
}

