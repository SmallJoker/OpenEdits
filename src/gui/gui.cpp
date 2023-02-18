#include "gui.h"
#include "client/client.h"
#include "server/server.h"
#include "core/blockmanager.h"
#include "core/macros.h"
#include <irrlicht.h>
#include <chrono>
// Scene handlers
#include "connect.h"
#include "lobby.h"
#include "gameplay.h"

extern const char *VERSION_STRING;

Gui::Gui()
{
	window_size = core::dimension2du(850, 550);

	SIrrlichtCreationParameters params;
	params.DriverType = video::EDT_OPENGL;
	params.Vsync = true;
	params.AntiAlias = 32;
	params.WindowSize = window_size;
	params.Stencilbuffer = false;
	params.EventReceiver = this;

	m_device = createDeviceEx(params);

	ASSERT_FORCED(m_device, "Failed to initialize driver");

	{
		// Title bar
		core::stringw version;
		core::multibyteToWString(version, VERSION_STRING);
		m_device->setWindowCaption(version.c_str());
	}

	scenemgr = m_device->getSceneManager();
	guienv = m_device->getGUIEnvironment();
	driver = m_device->getVideoDriver();

	{
		// Styling
		font = guienv->getFont("assets/fonts/DejaVuSans_15pt.png");
		ASSERT_FORCED(font, "Cannot load font");

		gui::IGUISkin *skin = guienv->getSkin();
		skin->setFont(font);
		auto make_opaque = [&skin] (gui::EGUI_DEFAULT_COLOR what) {
			auto color = skin->getColor(what);
			u32 alpha = color.getAlpha();
			if (alpha > 220)
				return;

			color.setAlpha((alpha + 2 * 0xFF + 1) / 3);
			skin->setColor(what, color);
		};
		make_opaque(gui::EGDC_3D_DARK_SHADOW);
		make_opaque(gui::EGDC_3D_FACE); // Tab header
		make_opaque(gui::EGDC_3D_SHADOW);
		make_opaque(gui::EGDC_3D_LIGHT);
		skin->setColor(gui::EGDC_HIGH_LIGHT, 0xDD113388); // selected items
		skin->setColor(gui::EGDC_3D_HIGH_LIGHT, 0xCC666666); // list 3D sunken pane BG
	}

	{
		// Scene handler registration
		registerHandler(SceneHandlerType::Connect, new SceneConnect());
		registerHandler(SceneHandlerType::Lobby, new SceneLobby());
		registerHandler(SceneHandlerType::Gameplay, new SceneGameplay());
	}

	m_scenetype = SceneHandlerType::Connect;
	m_scenetype_next = SceneHandlerType::CTRL_RENEW;

	ASSERT_FORCED(g_blockmanager, "Missing BlockManager");
	g_blockmanager->populateTextures(driver);

	m_initialized = true;
}

Gui::~Gui()
{
	// Avoid double-frees caused by callback chains
	setEventHandler(nullptr);

	delete m_client;
	delete m_server;

	for (auto it : m_handlers)
		delete it.second;
	m_handlers.clear();

	delete m_device;
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
		}

		if (m_scenetype_next == SceneHandlerType::CTRL_RENEW) {
			m_scenetype_next = m_scenetype;
		} else if (m_scenetype_next != m_scenetype) {
			// Substitutes for the constructors that do not allow access to "Gui *"
			getHandler(m_scenetype)->OnClose();
			m_scenetype = m_scenetype_next;
			getHandler(m_scenetype)->OnOpen();
		}

		driver->beginScene(true, true, video::SColor(0xFF000000));

		auto handler = getHandler(m_scenetype);

		if (is_new_screen) {
			handler->draw();
			// TODO: draw popups here to appear above everything else
		}

		handler->step(dtime);
		scenemgr->drawAll();
		guienv->drawAll();

		drawFPS();
		drawPopup(dtime);

		driver->endScene();
	}

	scenemgr->clear();
	guienv->clear();
}


bool Gui::OnEvent(const SEvent &event)
{
	if (!m_initialized)
		return false;

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
	auto it = m_handlers.find(m_scenetype);
	ASSERT_FORCED(it != m_handlers.end(), "Unknown handler");
	return it->second;
}

void Gui::connect(SceneConnect *sc)
{
	ASSERT_FORCED(!m_server && !m_client, "Already initialized");

	ClientStartData init;

	if (sc->start_localhost) {
		m_server = new Server();
		init.address = "127.0.0.1";
	}

	utf32_to_utf8(init.address, sc->address.c_str());
	utf32_to_utf8(init.nickname, sc->nickname.c_str());


	m_client = new Client(init);
	m_client->setEventHandler(this);

	for (int i = 0; i < 10 && m_client->getState() != ClientState::LobbyIdle; ++i) {
		sleep_ms(200);
	}

	if (m_client->getState() == ClientState::LobbyIdle) {
		setNextScene(SceneHandlerType::Lobby);

		GameEvent e(GameEvent::G2C_LOBBY_REQUEST);
		sendNewEvent(e);
	} else {
		if (m_popup_text.empty())
			showPopupText("Connection timed out: Server is not reachable.");
		m_pending_disconnect = true;
	}
}

void Gui::disconnect()
{
	setNextScene(SceneHandlerType::Connect);

	m_pending_disconnect = true;
}

void Gui::joinWorld(SceneLobby *sc)
{
	// Similar to the "Connect" scene. More fields might be added to create and delete worlds
	GameEvent e(GameEvent::G2C_JOIN);
	e.text = new std::string(sc->world_id);
	sendNewEvent(e);
}

void Gui::leaveWorld()
{
	GameEvent e(GameEvent::G2C_LEAVE);
	sendNewEvent(e);
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

	// Pin to the bottom (maybe not needed)
	if (pos.X + size.Width < 0)
		pos.X = -size.Width;
	else if (pos.X + size.Width >= (s32)window_size.Width)
		pos.X = window_size.Width - size.Width - 1;

	if (pos.Y + size.Height < 0)
		pos.Y = -size.Height;
	else if (pos.Y + size.Height >= (s32)window_size.Height)
		pos.Y = window_size.Height - size.Height - 1;

	ret.UpperLeftCorner = pos;
	ret.LowerRightCorner = pos + size;

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
	int fps = driver->getFPS();
	core::stringw str;
	core::multibyteToWString(str, std::to_string(fps).c_str());
	core::recti rect(
		core::vector2di(window_size.Width - 40, window_size.Height - 20),
		core::dimension2di(50, 50)
	);
	font->draw(str, rect, 0xFFFFFF00);
}


void Gui::showPopupText(const std::string &str)
{
	printf("GUI popup: %s\n", str.c_str());

	std::wstring wstr;
	utf8_to_utf32(wstr, str.c_str());

	m_popup_timer += 7;
	if (m_popup_text.empty()) {
		m_popup_text = wstr.c_str();
	} else {
		m_popup_text += L"\n";
		m_popup_text += wstr.c_str();
	}
}


void Gui::drawPopup(float dtime)
{
	if (m_popup_timer <= 0) {
		m_popup_text.clear();
		return;
	}

	core::recti rect = getRect({25, 80}, {50, 10});
	video::SColor color(0xFFFFFF00);
	if (m_popup_timer < 1.0f) {
		color.setAlpha(255 * m_popup_timer);
	}

	font->draw(m_popup_text, rect, color, true, true);

	m_popup_timer -= dtime;
}
