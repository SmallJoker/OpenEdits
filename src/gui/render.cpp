#include "render.h"
#include "core/macros.h"
#include <irrlicht.h>
#include <chrono>
// Scene handlers
#include "connect.h"

using namespace irr;

Render::Render()
{
	device = createDevice(video::EDT_OPENGL,
		core::dimension2d<u32>(640, 480), 16,
		false, false, true /* vsync */, this);

	ASSERT_FORCED(device, "Failed to initialize driver");

	device->setWindowCaption(L"OpenEdits - development");

	scene = device->getSceneManager();
	gui = device->getGUIEnvironment();
	font = gui->getFont("assets/fonts/DejaVuSans_15pt.png");

	{
		// Scene handler registration
		registerHandler(SceneHandlerType::Connect, new SceneHandlerConnect());
	}

	m_type = SceneHandlerType::Connect;
	m_initialized = true;
}

Render::~Render()
{
	for (auto it : m_handlers)
		delete it.second;
}

// -------------- Public members -------------

void Render::run()
{
	video::IVideoDriver *driver = device->getVideoDriver();
	auto t_last = std::chrono::steady_clock::now();

	bool is_new_screen = true;
	while (device->run() && m_type != SceneHandlerType::CTRL_QUIT) {
		// Measure precise timings
		auto t_now = std::chrono::steady_clock::now();
		float dtime = std::chrono::duration<float>(t_now - t_last).count();
		t_last = t_now;

		if (is_new_screen) {
			// Clear GUI elements
			scene->clear();
			gui->clear();
		}

		driver->beginScene(true, true, video::SColor(0xFF000000));

		auto it = m_handlers.find(m_type);
		ASSERT_FORCED(it != m_handlers.end(), "Unknown handler");

		if (is_new_screen)
			it->second->draw();

		scene->drawAll();
		gui->drawAll();

		SceneHandlerType type_next = it->second->step(dtime);
		if (type_next != SceneHandlerType::CTRL_NOOP) {
			is_new_screen = (type_next != m_type);
			m_type = type_next;
		} else {
			is_new_screen = false;
		}

		driver->endScene();
	}
}


bool Render::OnEvent(const SEvent &event)
{
	if (!m_initialized)
		return false;

	auto it = m_handlers.find(m_type);
	return it->second->OnEvent(event);
}


void Render::registerHandler(SceneHandlerType type, SceneHandler *handler)
{
	ASSERT_FORCED(m_handlers.find(type) == m_handlers.end(), "Already registered");

	m_handlers.insert({type, handler});
	handler->init(this);
}
