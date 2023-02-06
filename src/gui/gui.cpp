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
	m_window_size = core::dimension2du(700, 500);

	device = createDevice(video::EDT_OPENGL,
		m_window_size, 32, false, false, true /* vsync */, this);

	ASSERT_FORCED(device, "Failed to initialize driver");

	device->setWindowCaption(L"OpenEdits - development");

	scenemgr = device->getSceneManager();
	gui = device->getGUIEnvironment();
	driver = device->getVideoDriver();

	{
		// Styling
		font = gui->getFont("assets/fonts/DejaVuSans_15pt.png");
		ASSERT_FORCED(font, "Cannot load font");

		gui::IGUISkin *skin = gui->getSkin();
		skin->setFont(font);
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
	for (auto it : m_handlers)
		delete it.second;

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
		if (screensize != m_window_size) {
			is_new_screen = true;
			m_window_size = screensize;
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
		gui->drawAll();

		handler->step(dtime);

		driver->endScene();

		if (m_client)
			m_client->step(dtime);
		if (m_server)
			m_server->step(dtime);
	}
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
		m_window_size.Width * pos_perc.X * 0.01f,
		m_window_size.Height * pos_perc.Y * 0.01f
	);

	// Pin to the bottom (maybe not needed)
	if (pos.X + size.Width < 0)
		pos.X = -size.Width;
	else if (pos.X + size.Width >= (s32)m_window_size.Width)
		pos.X = m_window_size.Width - size.Width - 1;

	if (pos.Y + size.Height < 0)
		pos.Y = -size.Height;
	else if (pos.Y + size.Height >= (s32)m_window_size.Height)
		pos.Y = m_window_size.Height - size.Height - 1;

	ret.UpperLeftCorner = pos;
	ret.LowerRightCorner = pos + size;

	if (size.Width < 0 || size.Height < 0)
		ret.repair();

	return ret;
}

void Gui::displaceRect(core::recti &rect, core::vector2df pos_perc)
{
	core::vector2di disp(
		m_window_size.Width * pos_perc.X * 0.01f,
		m_window_size.Height * pos_perc.Y * 0.01f
	);
	rect += disp;
}

