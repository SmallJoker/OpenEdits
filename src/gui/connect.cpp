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
	int num = (rand() % 899) + 100;
	m_login.nickname = L"Guest" + std::to_wstring(num);
	m_login.address = L"127.0.0.1";
}

// -------------- Public members -------------

void SceneConnect::OnClose()
{
}

void SceneConnect::draw()
{
	using namespace guilayout;
	using WRAP = guilayout::IGUIElementWrapper;

	Table &root = *m_gui->layout;
	root.clear();
	root.setSize(3, 5);
	root.col(0)->weight = 20; // left
	root.col(2)->weight = 20; // right
	root.row(4)->weight = 20; // bottom

	auto gui = m_gui->guienv;
	core::recti norect;
	core::vector2di nopos;

	{
		// Logo
		auto texture = gui->getVideoDriver()->getTexture("assets/logo.png");
		auto dim = texture->getOriginalSize();

		auto *i_img = gui->addImage(texture, nopos, false);
		auto *g_img = root.add<WRAP>(1, 1, i_img);
		g_img->margin = { 1, 1, 1, 1 };
		g_img->expand = { 0, 0 };
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
		table_prompt->add<WRAP>(0, column, i_text);

		auto *i_box = gui->addEditBox(
			m_login.nickname.c_str(), norect, true, nullptr, ID_BoxNickname);
		table_prompt->add<WRAP>(1, column, i_box);

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnHost, L"Host");
		table_prompt->add<WRAP>(2, column, i_btn);

		column++;
	}

	{
		auto i_text = gui->addStaticText(L"Password", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		table_prompt->add<WRAP>(0, column, i_text);

		auto *i_box = gui->addEditBox(
			m_login.password.c_str(), norect, true, nullptr, ID_BoxPassword);
		i_box->setPasswordBox(true);
		table_prompt->add<WRAP>(1, column, i_box);

		column++;
	}

	{
		auto i_text = gui->addStaticText(L"Address", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		table_prompt->add<WRAP>(0, column, i_text);

		core::stringw str;

		auto *i_box = gui->addEditBox(
			m_login.address.c_str(), norect, true, nullptr, ID_BoxAddress);
		table_prompt->add<WRAP>(1, column, i_box);

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnConnect, L"Connect");
		table_prompt->add<WRAP>(2, column, i_btn);

		column++;
	}

	// Server selector
	root.row(3)->weight = 20;
	Table *table_srv = root.add<Table>(1, 3);
	table_srv->setSize(2, 1);
	table_srv->col(1)->weight = 30; // listbox

	{
		FlexBox *box_left = table_srv->add<FlexBox>(0, 0);
		box_left->box_axis = Element::SIZE_Y;
		box_left->allow_wrap = false; // H center

		// Server selector dropdown
		auto i_text = gui->addStaticText(L"Servers", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		box_left->add<WRAP>(i_text);

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnDelServer, L"Remove");
		auto *g_btn = box_left->add<WRAP>(i_btn);


		auto *i_list = gui->addListBox(norect, nullptr, ID_ListServers, true);
		auto *g_list = table_srv->add<WRAP>(1, 0, i_list);

		g_btn->margin = { 0, 20, 1, 1 };
		g_list->margin = { 1, 1, 1, 1 };
		g_list->min_size = { 300, 100 };

		updateServers();
	}
}

void SceneConnect::step(float dtime)
{
	using namespace guilayout;

	if (0) {
		// For debugging
		m_gui->layout->doRecursive([this] (Element *e) -> bool {
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
		s32 id = e.GUIEvent.Caller->getID();
		switch (e.GUIEvent.EventType) {
			case gui::EGET_BUTTON_CLICKED:
				{
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
				if (id == ID_ListServers) {
					gui::IGUIListBox *listbox = (gui::IGUIListBox *)e.GUIEvent.Caller;

					try {
						auto info = m_index_to_address.at(listbox->getSelected());
						m_login.address = info.address;
						m_login.nickname = info.nickname;

						m_gui->requestRenew();
					} catch (std::exception &) {
						break;
					}
				}
				break;
			case gui::EGET_EDITBOX_ENTER:
				if (id == ID_BoxPassword) {
					std::string out;
					wide_to_utf8(out, e.GUIEvent.Caller->getText());
					out = strtrim(out);
					// Try to guess whether to host a server or connect to one.
					// Hosting falls back to client mode, which is not nice but works well enough.
					if (out.empty() || out == "127.0.0.1" || out == "localhost")
						onSubmit(ID_BtnHost);
					else
						onSubmit(ID_BtnConnect);
					break;
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

void SceneConnect::recordLogin(ClientStartData data)
{
	for (char &c : data.address)
		c = tolower(c);
	to_player_name(data.nickname);

	if (data.nickname.rfind("GUEST", 0) == 0)
		return;

	std::ifstream is("client_servers.txt");
	std::string line;
	while (std::getline(is, line)) {
		LoginInfo info;
		std::string address_f = get_next_part(line);
		std::string nickname_f =  get_next_part(line);
		if (address_f == data.address && nickname_f == data.nickname)
			return;
	}
	is.close();

	// Append to the end of the list
	{
		std::ofstream os("client_servers.txt", std::ios_base::app);
		os << data.address << " " << data.nickname << std::endl;
		os.close();
	}
}


void SceneConnect::onSubmit(int elementid)
{
	auto root = m_gui->guienv->getRootGUIElement();

	// To restore the input boxes on failure
	m_login.nickname = root->getElementFromId(ID_BoxNickname)->getText();
	m_login.password = root->getElementFromId(ID_BoxPassword)->getText();
	m_login.address = root->getElementFromId(ID_BoxAddress)->getText();

	// Send connection request
	ClientStartData start_data;

	wide_to_utf8(start_data.nickname, m_login.nickname.c_str());
	wide_to_utf8(start_data.password, m_login.password.c_str());
	if (elementid != ID_BtnHost)
		wide_to_utf8(start_data.address, m_login.address.c_str());
	else
		; // empty address == host local server

	m_gui->connect(start_data);
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
		if (info.address == m_login.address.c_str() && info.nickname == m_login.nickname.c_str())
			listbox->setSelected(i);
	}
}

