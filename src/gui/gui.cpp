#include "gui.h"
#include "client/client.h"
#include "server/server.h"
#include "core/blockmanager.h"
#include "core/macros.h"
#include "guilayout/guilayout_irrlicht.h"
#include <irrlicht.h>
#include <chrono>
// Scene handlers
#include "connect.h"
#include "register.h"
#include "loading.h"
#include "lobby.h"
#include "gameplay/gameplay.h"
#include "version.h"

Gui::Gui()
{
	window_size = core::dimension2du(850, 550);

	SIrrlichtCreationParameters params;
	params.DriverType = video::EDT_OPENGL;
	params.Vsync = true;
	//params.AntiAlias = 32; -- only does something on Windows?
	params.WindowSize = window_size;
	params.Stencilbuffer = false;
	params.EventReceiver = this;
	params.WindowResizable = 1; // Haiku (SDL2) does not like = 2

	m_device = createDeviceEx(params);

	ASSERT_FORCED(m_device, "Failed to initialize driver");

	scenemgr = m_device->getSceneManager();
	guienv = m_device->getGUIEnvironment();
	driver = m_device->getVideoDriver();
	layout = new guilayout::Table();

	{
		// Styling
		font = guienv->getFont("assets/fonts/DejaVuSans_15pt.png");
		ASSERT_FORCED(font, "Cannot load font");

		gui::IGUISkin *skin = guienv->getSkin();
		skin->setFont(font);

		skin->setColor(gui::EGDC_HIGH_LIGHT, 0xFF3344EE); // text highlight
		skin->setColor(gui::EGDC_EDITABLE, 0xFFDDDDDD); // edit box bg
		skin->setColor(gui::EGDC_FOCUSED_EDITABLE, 0xFFBBDDFF); // edit box bg
		skin->setColor(gui::EGDC_BUTTON_TEXT, 0xFF000000);
		skin->setColor(gui::EGDC_3D_DARK_SHADOW, 0xDD999999); // button bottom/right
		skin->setColor(gui::EGDC_3D_HIGH_LIGHT, 0xDDEEEEEE); // button top/left & list background
		skin->setColor(gui::EGDC_3D_SHADOW, 0xDD666666); // button 2nd level bottom/right
		skin->setColor(gui::EGDC_3D_FACE, 0xDDCCCCCC); // button face (interpolated)

		skin->setSize(gui::EGDS_SCROLLBAR_SIZE, 24);
	}

	{
		// Scene handler registration
		registerHandler(SceneHandlerType::Connect, new SceneConnect());
		registerHandler(SceneHandlerType::Loading, new SceneLoading());
		registerHandler(SceneHandlerType::Register, new SceneRegister());
		registerHandler(SceneHandlerType::Lobby, new SceneLobby());
		registerHandler(SceneHandlerType::Gameplay, new SceneGameplay());
	}

	m_scenetype = SceneHandlerType::Connect;
	m_scenetype_next = SceneHandlerType::CTRL_RENEW;

	setWindowTitle();

	ASSERT_FORCED(g_blockmanager, "Missing BlockManager");

	m_initialized = true;
}

Gui::~Gui()
{
	// Avoid double-frees caused by callback chains
	setEventHandler(nullptr);

	delete m_client;
	delete m_server;
	delete layout;

	for (auto it : m_handlers)
		delete it.second;
	m_handlers.clear();

	m_device->drop();
}

// -------------- Public members -------------

void Gui::run()
{
	auto t_last = std::chrono::steady_clock::now();
	getHandler(m_scenetype)->OnOpen();

	while (m_device->run() && m_scenetype_next != SceneHandlerType::CTRL_QUIT) {
		float dtime;
		{
			// Measure precise timings
			auto t_now = std::chrono::steady_clock::now();
			dtime = std::chrono::duration<float>(t_now - t_last).count();
			t_last = t_now;
		}

		if (m_pending_disconnect) {
			if (m_client) {
				delete m_client;
				m_client = nullptr;
			}
			if (m_server) {
				delete m_server;
				m_server = nullptr;
			}
			m_pending_disconnect = false;
		}

		if (m_client)
			m_client->step(dtime);
		if (m_server)
			m_server->step(dtime);

		bool is_new_screen = (m_scenetype_next != m_scenetype);

		auto screensize = driver->getScreenSize();
		if (screensize != window_size) {
			is_new_screen = true;
			window_size = screensize;
		}

		if (is_new_screen) {
			//printf("Renew scene type %d\n", (int)m_scenetype);

			// Clear GUI elements
			scenemgr->clear();
			guienv->clear();
			layout->clear();
		}

		if (m_scenetype_next == SceneHandlerType::CTRL_RENEW) {
			m_scenetype_next = m_scenetype;
		} else if (m_scenetype_next != m_scenetype) {
			// Substitutes for the constructors that do not allow access to "Gui *"
			getHandler(m_scenetype)->OnClose();
			m_scenetype = m_scenetype_next;
			getHandler(m_scenetype)->OnOpen();
			setWindowTitle();
		}

		driver->beginScene(true, true, video::SColor(0xFF000000));

		auto handler = getHandler(m_scenetype);

		if (is_new_screen) {
			handler->draw();
			layout->start({0, 0}, {(u16)window_size.Width, (u16)window_size.Height});
		}

		handler->step(dtime);
		scenemgr->drawAll();
		guienv->drawAll();

		if (m_show_debug)
			guilayout::IGUIElementWrapper::debugFillArea(layout, driver, 0xFFFF0000);

		drawFPS();
		drawPopup(dtime);

		driver->endScene();
	}

	getHandler(m_scenetype)->OnClose();
	scenemgr->clear();
	guienv->clear();
	puts("Gui: Terminated properly.");
}


bool Gui::OnEvent(const SEvent &event)
{
	if (!m_initialized)
		return false;

	if (event.EventType == EET_KEY_INPUT_EVENT) {
		if (event.KeyInput.Key == KEY_F1 && event.KeyInput.PressedDown) {
			m_show_debug ^= true;
		}
	}

	return getHandler(m_scenetype)->OnEvent(event);
}

bool Gui::OnEvent(GameEvent &e)
{
	if (!m_initialized)
		return false;

	using E = GameEvent::C2G_Enum;
	switch (e.type_c2g) {
		case E::C2G_DISCONNECT:
			disconnect();
			showPopupText("Disconnected from the server");
			return true;
		case E::C2G_CHANGE_PASS:
			setNextScene(SceneHandlerType::Lobby);
			return true;
		case E::C2G_JOIN:
			setNextScene(SceneHandlerType::Gameplay);
			return true;
		case E::C2G_LEAVE:
			setNextScene(SceneHandlerType::Lobby);
			return true;
		case E::C2G_DIALOG:
			showPopupText(*e.text);
			break;
		default: break;
	}

	return getHandler(m_scenetype)->OnEvent(e);
}


void Gui::registerHandler(SceneHandlerType type, SceneHandler *handler)
{
	ASSERT_FORCED(m_handlers.find(type) == m_handlers.end(), "Already registered");

	m_handlers.insert({type, handler});
	handler->m_gui = this;
}

SceneHandler *Gui::getHandler(SceneHandlerType type)
{
	auto it = m_handlers.find(type);
	ASSERT_FORCED(it != m_handlers.end(), "Unknown handler");
	return it->second;
}

void Gui::setWindowTitle()
{
	core::stringw version;
	core::multibyteToWString(version, VERSION_STRING);
	auto it = m_handlers.find(m_scenetype);
	if (it != m_handlers.end()) {
		version.append(L" - ");
		version.append(it->second->m_scene_name);
	}

	m_device->setWindowCaption(version.c_str());
}

io::IFileSystem *Gui::getFileSystem()
{
	return m_device->getFileSystem();
}

void Gui::setSceneFromClientState()
{
	ClientState state = ClientState::None;
	if (m_client)
		state = m_client->getState();

	switch (state) {
	case ClientState::None:
		break;
	case ClientState::Register:
		setNextScene(SceneHandlerType::Register);
		break;
	case ClientState::LobbyIdle:
		setNextScene(SceneHandlerType::Lobby);
		break;
	case ClientState::WorldPlay:
		setNextScene(SceneHandlerType::Gameplay);
		break;
	case ClientState::Connected:
	case ClientState::WorldJoin:
		return; // in-between states. not mapped.
	case ClientState::Invalid:
		assert(false);
		break;
	}
}


bool Gui::connect(ClientStartData &init)
{
	if (m_server || m_client) {
		showPopupText("Already initialized");
		return false;
	}

	if (init.address.empty()) {
		m_server = new Server(&m_pending_disconnect);
		init.address = "127.0.0.1";
	}

	g_blockmanager->setDriver(driver);
	g_blockmanager->doPackRegistration(); // TODO: replace with server-sent script

	m_client = new Client(init);
	m_client->setEventHandler(this);

	setNextScene(SceneHandlerType::Loading);
	SceneLoading *sc = (SceneLoading *)getHandler(SceneHandlerType::Loading);
	if (sc)
		sc->loading_type = SceneLoading::Type::ConnectServer;
	return true;
}


void Gui::disconnect()
{
	setNextScene(SceneHandlerType::Connect);

	m_pending_disconnect = true;
}

void Gui::joinWorld(SceneLobby *sc)
{
	joinWorld(sc->world_id);
}

void Gui::joinWorld(const std::string &world_id)
{
	// Similar to the "Connect" scene. More fields might be added to create and delete worlds
	if (world_id.empty())
		return;

	GameEvent e(GameEvent::G2C_JOIN);
	e.text = new std::string(world_id);
	sendNewEvent(e);
}

void Gui::leaveWorld()
{
	GameEvent e(GameEvent::G2C_LEAVE);
	sendNewEvent(e);
}

void Gui::setSceneLoggedIn(SceneHandlerType type)
{
	if (m_client->getState() != ClientState::LobbyIdle)
		return;

	using T = SceneHandlerType;
	switch (type) {
		case T::Register:
		case T::Lobby:
			break;
		default: return; // not accepted
	}
	setNextScene(type);
}


// -------------- GUI utility functions -------------

core::recti Gui::getRect(core::vector2df pos_perc, core::dimension2di size_perc)
{
	core::recti ret;

	core::vector2di pos(
		window_size.Width * pos_perc.X * 0.01f,
		window_size.Height * pos_perc.Y * 0.01f
	);

	// negative size: treat as pixel value
	core::dimension2di size;
	if (size_perc.Width <= 0)
		size.Width = -size_perc.Width;
	else
		size.Width = window_size.Width * size_perc.Width * 0.01f;

	if (size_perc.Height <= 0)
		size.Height = -size_perc.Height;
	else
		size.Height = window_size.Height * size_perc.Height * 0.01f;

	// Pin to the bottom
	if (pos.X < 0) {
		pos.X += window_size.Width;
		size.Width *= -1;
	}
	if (pos.Y < 0) {
		pos.Y += window_size.Height;
		size.Height *= -1;
	}

	ret.UpperLeftCorner = pos;
	ret.LowerRightCorner = pos + size;
	ret.repair();

	return ret;
}

void Gui::displaceRect(core::recti &rect, core::vector2df pos_perc)
{
	core::vector2di disp(
		window_size.Width * pos_perc.X * 0.01f,
		window_size.Height * pos_perc.Y * 0.01f
	);
	rect += disp;
}

// -------------- Overlay elements -------------

void Gui::drawFPS()
{
	// FPS indicator text on the bottom right
	{
		int fps = driver->getFPS();
		core::stringw str;
		core::multibyteToWString(str, std::to_string(fps).c_str());
		core::recti rect(
			core::vector2di(window_size.Width - 40, 5),
			core::dimension2di(50, 50)
		);
		font->draw(str, rect, 0xFFFFFF00);
	}

	// Debug info
	if (m_show_debug && m_client) {
		core::stringw str;

		core::multibyteToWString(str, m_client->getDebugInfo().c_str());
		auto dim = font->getDimension(str.c_str());

		core::recti rect(
			core::vector2di(5, 5),
			dim
		);

		core::vector2di border(2, 2);
		core::recti rect2 = rect;
		rect2.UpperLeftCorner -= border;
		rect2.LowerRightCorner += border;

		guienv->getSkin()->draw2DRectangle(nullptr, 0x77222222, rect2);
		font->draw(str, rect, 0xFFFFFF00);
	}
}


void Gui::showPopupText(const std::string &str)
{
	PopupEntry e;
	utf8_to_wide(e.text, str.c_str());

	bool found = false;
	for (auto it = m_popup_entries.begin(); it != m_popup_entries.end(); ++it) {
		if (it->text != e.text)
			continue;

		// Found match
		found = true;
		m_popup_entries.erase(it);
		break;
	}

	e.expiry = 7;
	m_popup_entries.emplace_back(std::move(e));

	if (!found)
		fprintf(stderr, "Gui popup: %s\n", str.c_str());
}


void Gui::drawPopup(float dtime)
{
	if (m_popup_entries.empty())
		return;

	std::wstring all_text;
	all_text.reserve(255);
	float max_time = 0;

	for (auto it = m_popup_entries.begin(); it != m_popup_entries.end();) {
		it->expiry -= dtime;
		if (it->expiry <= 0) {
			it = m_popup_entries.erase(it);
			continue;
		}

		if (!all_text.empty())
			all_text.append(L"\n");
		all_text.append(it->text);
		max_time = std::max<float>(max_time, it->expiry);
		it++;
	}

	if (all_text.empty())
		return;

	auto dim = font->getDimension(all_text.c_str());
	core::recti rect = getRect({50, 80}, {5, 5});
	rect.UpperLeftCorner -= core::vector2di(dim.Width, dim.Height) / 2;
	rect.LowerRightCorner += core::vector2di(dim.Width, dim.Height) / 2;

	video::SColor color(0xFFFFFF00);
	video::SColor color_frame(0xFF444444);
	if (max_time < 1.0f) {
		color.setAlpha(255 * max_time);
		color_frame.setAlpha(255 * max_time);
	}

	guienv->getSkin()->draw2DRectangle(nullptr, color_frame, rect);
	font->draw(all_text.c_str(), rect, color, true, true);
}
