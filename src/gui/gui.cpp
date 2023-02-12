#include "gui.h"
#include "client/client.h"
#include "server/server.h"
#include "core/macros.h"
#include <irrlicht.h>
#include <chrono>
// Scene handlers
#include "connect.h"
#include "lobby.h"
#include "gameplay.h"

void sleep_ms(long delay);

Gui::Gui()
{
	window_size = core::dimension2du(850, 550);

	device = createDevice(video::EDT_OPENGL,
		window_size, 32, false, false, true /* vsync */, this);

	ASSERT_FORCED(device, "Failed to initialize driver");

	device->setWindowCaption(L"OpenEdits v1.0.2-dev");

	scenemgr = device->getSceneManager();
	gui = device->getGUIEnvironment();
	driver = device->getVideoDriver();

	{
		// Styling
		font = gui->getFont("assets/fonts/DejaVuSans_15pt.png");
		ASSERT_FORCED(font, "Cannot load font");

		gui::IGUISkin *skin = gui->getSkin();
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
		make_opaque(gui::EGDC_3D_HIGH_LIGHT);
		make_opaque(gui::EGDC_3D_LIGHT);
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

	delete device;
}

// -------------- Public members -------------

void Gui::run()
{
	auto t_last = std::chrono::steady_clock::now();

	while (device->run() && m_scenetype_next != SceneHandlerType::CTRL_QUIT) {
		float dtime;
		{
			// Measure precise timings
			auto t_now = std::chrono::steady_clock::now();
			dtime = std::chrono::duration<float>(t_now - t_last).count();
			t_last = t_now;
		}

		if (m_client) {
			m_client->step(dtime);
		}
		if (m_server) {
			m_server->step(dtime);
		}

		bool is_new_screen = (m_scenetype_next != m_scenetype);

		if (m_scenetype_next == SceneHandlerType::CTRL_RENEW)
			m_scenetype_next = m_scenetype;
		else
			m_scenetype = m_scenetype_next;

		auto screensize = driver->getScreenSize();
		if (screensize != window_size) {
			is_new_screen = true;
			window_size = screensize;
		}

		if (is_new_screen) {
			//printf("Renew scene type %d\n", (int)m_scenetype);

			// Clear GUI elements
			scenemgr->clear();
			gui->clear();
		}

		driver->beginScene(true, true, video::SColor(0xFF000000));

		auto handler = getHandler(m_scenetype);

		if (is_new_screen)
			handler->draw();

		scenemgr->drawAll();
		handler->step(dtime);
		gui->drawAll();

		{
			int fps = driver->getFPS();
			core::stringw str;
			core::multibyteToWString(str, std::to_string(fps).c_str());
			core::recti rect(
				core::vector2di(window_size.Width - 40, window_size.Height - 20),
				core::dimension2di(50, 50)
			);
			font->draw(str, rect, 0xFFFFFF00);
		}

		driver->endScene();

		if (m_client)
			m_client->step(dtime);
		if (m_server)
			m_server->step(dtime);
	}

	scenemgr->clear();
	gui->clear();
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

	return getHandler(m_scenetype)->OnEvent(e);
}


void Gui::registerHandler(SceneHandlerType type, SceneHandler *handler)
{
	ASSERT_FORCED(m_handlers.find(type) == m_handlers.end(), "Already registered");

	m_handlers.insert({type, handler});
	handler->init(this);
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

	{
		core::stringc str;
		wStringToMultibyte(str, sc->address);
		init.address = str.c_str();
	}
	{
		core::stringc str;
		wStringToMultibyte(str, sc->nickname);
		init.nickname = str.c_str();
	}

	m_client = new Client(init);

	for (int i = 0; i < 10 && m_client->getState() != ClientState::LobbyIdle; ++i) {
		sleep_ms(200);
	}

	if (m_client->getState() == ClientState::LobbyIdle) {
		m_client->setEventHandler(this);
		setNextScene(SceneHandlerType::Lobby);
	} else {
		puts("Wait timed out.");
		delete m_client;
		m_client = nullptr;

		delete m_server;
		m_server = nullptr;
	}
}

void Gui::disconnect()
{
	setNextScene(SceneHandlerType::Connect);

	if (m_client) {
		delete m_client;
		m_client = nullptr;
	}

	if (m_server) {
		delete m_server;
		m_server = nullptr;
	}
}

void Gui::joinWorld(SceneLobby *sc)
{
	setNextScene(SceneHandlerType::Gameplay);
}

void Gui::leaveWorld()
{
	setNextScene(SceneHandlerType::Lobby);
}


// -------------- GUI utility functions -------------

core::recti Gui::getRect(core::vector2df pos_perc, core::dimension2di size)
{
	core::recti ret;

	core::vector2di pos(
		window_size.Width * pos_perc.X * 0.01f,
		window_size.Height * pos_perc.Y * 0.01f
	);

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

	if (size.Width < 0 || size.Height < 0)
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

void wStringToMultibyte(std::string &dst, const wchar_t *src)
{
	size_t length = wcslen(src);
	dst.resize(length);

	wcstombs(&dst[0], src, length);
}
