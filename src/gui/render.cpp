#include "render.h"
#include "core/macros.h"
#include <irrlicht.h>
#include <chrono>
// Scene handlers
#include "connect.h"

using namespace irr;

Render::Render()
{
	IrrlichtDevice *device = createDevice(video::EDT_OPENGL,
		core::dimension2d<u32>(640, 480), 16,
		false, false, true /* vsync */, this);

	ASSERT_FORCED(device, "Failed to initialize driver");

	device->setWindowCaption(L"OpenEdits - development");

	scene = device->getSceneManager();
	gui = device->getGUIEnvironment();
	video::IVideoDriver *driver = device->getVideoDriver();

	font = gui->getFont("assets/fonts/DejaVuSans_15pt.png");

	{
		// Scene handler registration
		registerHandler(SceneHandlerType::Connect, new SceneHandlerConnect());
		m_initialized = true;
	}

	m_type = SceneHandlerType::Connect;
	auto t_last = std::chrono::steady_clock::now();

	while (device->run() && m_type != SceneHandlerType::QUIT) {
		auto t_now = std::chrono::steady_clock::now();
		float dtime = std::chrono::duration<float>(t_now - t_last).count();
		t_last = t_now;

		driver->beginScene(true, true, video::SColor(0xFF000000));

		auto it = m_handlers.find(m_type);
		ASSERT_FORCED(it != m_handlers.end(), "Unknown handler");

		SceneHandlerType type_next = it->second->runPre(dtime);
		if (type_next != SceneHandlerType::KEEP_PREVIOUS)
			m_type = type_next;

		scene->drawAll();
		gui->drawAll();

		it->second->runPost();

		driver->endScene();
	}
}

Render::~Render()
{
	for (auto it : m_handlers)
		delete it.second;
}

// -------------- Public members -------------

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

