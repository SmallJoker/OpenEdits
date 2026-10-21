#include "connect.h"
#include "core/auth.h"
#include "guilayout/guilayout_irrlicht.h"
#include <IGUIButton.h>
#include <IGUICheckBox.h>
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
	ID_ChkRemember,
	ID_ListServers,
	ID_BtnDelServer
};

SceneConnect::SceneConnect() :
	SceneHandler(L"Connect")
{
	int num = (rand() % 899) + 100;
	m_inputs.nickname = "Guest" + std::to_string(num);
	m_inputs.address = "127.0.0.1";
}

// -------------- Public members -------------

void SceneConnect::OnClose()
{
	m_index_to_address.clear();
}

void SceneConnect::draw()
{
	using namespace guilayout;
	using WRAP = guilayout::IGUIElementWrapper;

	Table &root = *m_gui->layout;
	root.setSize(1, 1);

	/// Logo and lower part
	Table &table_w1h2 = *root.add<Table>(0, 0);
	table_w1h2.setSize(1, 2);
	table_w1h2.margin = { 6, 3, 6, 4 };
	table_w1h2.expand = { 3, 3 };
	table_w1h2.row(1)->weight = 30;

	auto gui = m_gui->guienv;
	core::recti norect;
	core::vector2di nopos;

	{
		// Logo
		auto texture = gui->getVideoDriver()->getTexture("assets/logo.png");
		auto dim = texture->getOriginalSize();

		auto *i_img = gui->addImage(texture, nopos, true);
		auto *g_img = table_w1h2.add<WRAP>(0, 0, i_img);
		g_img->margin = { 1, 1, 1, 1 };
		g_img->expand = { 0, 0 };
		g_img->min_size = { (s16)dim.Width, (s16)dim.Height };
		//ge->fixed_aspect_ratio = true;
	}

	/// Left (login), space and right (server list)
	Table &table_LR = *table_w1h2.add<Table>(0, 1);
	table_LR.setSize(3, 1);
	table_LR.col(0)->weight = 40;

	/// Input boxes and buttons
	Table *table_prompt = table_LR.add<Table>(0, 0);
	table_prompt->setSize(2, 7);
	table_prompt->col(1)->weight = 15;

	u16 column = 0;

	auto to_wide = [](const std::string &str) {
		std::wstring wstr;
		utf8_to_wide(wstr, str.c_str());
		return wstr;
	};

	{
		auto *i_text = gui->addStaticText(L"Username", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		table_prompt->add<WRAP>(0, column, i_text);

		auto *i_box = gui->addEditBox(
			to_wide(m_inputs.nickname).c_str(), norect, true, nullptr, ID_BoxNickname);
		table_prompt->add<WRAP>(1, column, i_box);

		column++;
	}

	{
		auto i_text = gui->addStaticText(L"Password", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		table_prompt->add<WRAP>(0, column, i_text);

		auto *i_box = gui->addEditBox(
			to_wide(m_inputs.password).c_str(), norect, true, nullptr, ID_BoxPassword);
		i_box->setPasswordBox(true);
		table_prompt->add<WRAP>(1, column, i_box);

		column++;
	}

	if (1) {
		auto *i_chk = gui->addCheckBox(
			true, norect, nullptr, ID_ChkRemember);
		i_chk->setNotClipped(true);
		i_chk->setDrawBackground(true);
		i_chk->setDrawBorder(true);
		i_chk->setToolTipText(L"Only supported by servers >= 1.8.0.");
		table_prompt->add<WRAP>(1, column, i_chk);

		auto *i_text = gui->addStaticText(L"Remember",
			core::recti(40, 2, 200, 50), false, false, i_chk);
		i_text->setNotClipped(true);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);

		column++;
	} else {
		column++;
	}

	{

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnHost, L"Host server");
		table_prompt->add<WRAP>(1, column, i_btn);

		column += 2;
	}


	{
		auto i_text = gui->addStaticText(L"Server", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		table_prompt->add<WRAP>(0, column, i_text);

		auto *i_box = gui->addEditBox(
			to_wide(m_inputs.address).c_str(), norect, true, nullptr, ID_BoxAddress);
		table_prompt->add<WRAP>(1, column, i_box);

		column++;
	}

	{
		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnConnect, L"Connect");
		table_prompt->add<WRAP>(1, column, i_btn);

		column++;
	}

	for (size_t y = 0; y < column; ++y) {
		if (auto &e = table_prompt->get(1, y)) {
			e->margin[Element::DIR_LEFT] = 0;
			e->margin[Element::DIR_RIGHT] = 0;
		}
	}

	/// Server selector
	Table *table_srv = table_LR.add<Table>(2, 0);
	table_srv->setSize(1, 2);
	table_srv->row(0)->weight = 0; /// table_head

	{
		Table *table_head = table_srv->add<Table>(0, 0);
		table_head->setSize(2, 1);

		// Server selector dropdown
		auto i_text = gui->addStaticText(L"Servers", norect, false, false);
		i_text->setOverrideColor(Gui::COLOR_ON_BG);
		table_head->add<WRAP>(0, 0, i_text);

		auto *i_btn = gui->addButton(norect, nullptr, ID_BtnDelServer, L"Remove");
		auto *g_btn = table_head->add<WRAP>(1, 0, i_btn);

		auto *i_list = gui->addListBox(norect, nullptr, ID_ListServers, true);
		auto *g_list = table_srv->add<WRAP>(0, 1, i_list);

		g_btn->margin = { 10, 10, 0, 10 };
		g_btn->expand = { 1, 1 };
		g_list->margin = { 0, 1, 0, 1 };
		g_list->min_size = { 300, 100 };

		updateServers();
	}

	updatePasswordField(false);
}

void SceneConnect::step(float dtime)
{
	(void)dtime;
}

bool SceneConnect::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT) {
		const s32 id = e.GUIEvent.Caller->getID();
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
			case gui::EGET_LISTBOX_SELECTED_AGAIN: // doubleclick
				if (id == ID_ListServers) {
					if (m_inputs.token.empty()) {
						auto root = m_gui->guienv->getRootGUIElement();
						m_gui->guienv->setFocus(root->getElementFromId(ID_BoxPassword));
					} else {
						onSubmit(0);
					}
					break;
				}
				break;
			case gui::EGET_LISTBOX_CHANGED:
				if (id == ID_ListServers) {
					gui::IGUIListBox *listbox = (gui::IGUIListBox *)e.GUIEvent.Caller;

					try {
						m_inputs = m_index_to_address.at(listbox->getSelected());

						m_gui->requestRenew();
					} catch (std::exception &) {
						break;
					}
				}
				break;
			case gui::EGET_EDITBOX_ENTER:
				if (id == ID_BoxPassword) {
					onSubmit(0);
					break;
				}
				break;
			case gui::EGET_EDITBOX_CHANGED:
			case gui::EGET_ELEMENT_FOCUSED:
				if (id == ID_BoxPassword) {
					// Restore normal appearance
					updatePasswordField(true);
					break;
				}
				break;
			case gui::EGET_ELEMENT_FOCUS_LOST:
				if (id == ID_BoxPassword) {
					updatePasswordField(false);
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
	if (1) {
		printf("%s: addr=%s name=%s token=%d\n",
			__func__, data.address.c_str(), data.nickname.c_str(),
			(int)data.token.size()
		);

		Auth::dump_bytes("token", data.token);
	}

	for (char &c : data.address)
		c = tolower(c);
	to_player_name(data.nickname);

	if (data.nickname.rfind("GUEST", 0) == 0)
		return;

	std::ifstream is("client_servers.txt");
	std::string line;
	while (std::getline(is, line)) {
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

void SceneConnect::backupInputs()
{
	auto root = m_gui->guienv->getRootGUIElement();

	auto convert = [root](std::string &out, int element_id, bool trim_spaces) {
		wide_to_utf8(out, root->getElementFromId(element_id)->getText());
		if (trim_spaces)
			out = strtrim(out);
	};
	convert(m_inputs.nickname, ID_BoxNickname, true);
	convert(m_inputs.password, ID_BoxPassword, false);
	convert(m_inputs.address,  ID_BoxAddress, true);

	auto cb = (gui::IGUICheckBox *)root->getElementFromId(ID_ChkRemember);
	m_inputs.remember_password = cb && cb->isChecked();
}

std::string SceneConnect::getToken()
{
	// TODO

	//m_config_db->getToken();
	return "AA";
}

void SceneConnect::onSubmit(int elementid)
{
	backupInputs();

	bool do_host = false;
	switch (elementid) {
	case 0:
		{
			// Autodetect

			auto &addr = m_inputs.address;
			// Try to guess whether to host a server or connect to one.
			// Falls back to client mode when the port is occupied. Not nice but does work.
			if (addr.empty() || addr == "127.0.0.1" || addr == "localhost") {
				do_host = true;
				addr = "127.0.0.1"; // reference
			}
		}

		break;
	case ID_BtnHost:    do_host = true;  break;
	case ID_BtnConnect: do_host = false; break;
	default: return;
	}

	ClientStartData start_data = m_inputs;

	if (do_host)
		start_data.address.clear(); // empty address == host local server

	if (m_use_token) {
		start_data.token = getToken();
		start_data.password.clear();
	} else {
		start_data.token.clear();
	}

	m_gui->connect(start_data);
}

void SceneConnect::updatePasswordField(bool is_focused)
{
	auto root = m_gui->guienv->getRootGUIElement();
	auto pb = (gui::IGUIEditBox *)root->getElementFromId(ID_BoxPassword);
	if (!pb)
		return;

	auto cb = (gui::IGUICheckBox *)root->getElementFromId(ID_ChkRemember);

	if (is_focused) {
		if (m_use_token)
			pb->setText(L"");
		m_use_token = false;
	} else {
		if (pb->getText()[0] == L'\0') {
			// Empty -> Try to use token if available
			const bool have_token = !m_inputs.token.empty();
			if (have_token) {
				pb->setText(L"xxxxxxxxxxxx");
				if (cb)
					cb->setChecked(true);
			}
			m_use_token = have_token;
		} else {
			m_use_token = false;
		}
	}

	pb->setOverrideColor(0xFF555555);
	pb->enableOverrideColor(!is_focused && m_use_token);
	pb->setWritable(is_focused || !m_use_token);

	if (cb)
		cb->setEnabled(!m_use_token);
}


static const std::string SERVERS_FILE = "client_servers.txt";

static ClientStartData get_from_line(std::string &line)
{
	ClientStartData start_data;
	start_data.address = get_next_part(line).c_str();
	start_data.nickname = get_next_part(line).c_str();
	return start_data;
}

void SceneConnect::removeServer(int index)
{
	const ClientStartData &ref = m_index_to_address.at(index);

	// Read in the saved servers
	std::ifstream is(SERVERS_FILE);
	std::ofstream os(SERVERS_FILE + ".tmp", std::ios_base::trunc);
	std::string line;
	while (std::getline(is, line)) {
		const std::string original_line = line;

		ClientStartData info = get_from_line(line);
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
	m_index_to_address.clear();

	auto root = m_gui->guienv->getRootGUIElement();
	auto listbox = (gui::IGUIListBox *)root->getElementFromId(ID_ListServers);

	if (!listbox)
		return;

	{

		// Read in the saved servers
		std::ifstream is("client_servers.txt");
		std::string line;
		while (std::getline(is, line)) {
			ClientStartData info = get_from_line(line);
			if (info.nickname.empty())
				continue;

			// TODO: Check for token availability (while already reading from the database)
			info.token = false ? "Y" : "";
			m_index_to_address.push_back(std::move(info));
		}
	}

	// Add to the gui
	listbox->clear();
	for (auto &start_data : m_index_to_address) {
		std::wstring label;
		utf8_to_wide(label, (start_data.nickname + "  -  " + start_data.address).c_str());

		auto i = listbox->addItem(label.c_str());
		// TODO: "Invalid read of size 32" while doing string compare. Why?
		if (start_data.address == m_inputs.address && start_data.nickname == m_inputs.nickname)
			listbox->setSelected(i);
	}
}

