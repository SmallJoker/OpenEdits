#pragma once

#include <IEventReceiver.h>
#include <map>

namespace irr {
	class IrrlichtDevice;

	namespace gui {
		class IGUIEnvironment;
		class IGUIFont;
	}
	namespace scene {
		class ISceneManager;
	}
}

using namespace irr;

class SceneHandler;

enum class SceneHandlerType {
	Connect,
	Lobby,
	World,
	QUIT,
	KEEP_PREVIOUS
};

class Render : public IEventReceiver {
public:
	Render();
	~Render();

	bool OnEvent(const SEvent &event) override;

	void registerHandler(SceneHandlerType type, SceneHandler *handler);

	scene::ISceneManager *scene = nullptr;
	gui::IGUIEnvironment *gui = nullptr;
	gui::IGUIFont *font = nullptr;

private:
	bool m_initialized = false;
	SceneHandlerType m_type;
	IrrlichtDevice *device = nullptr;

	std::map<SceneHandlerType, SceneHandler *> m_handlers;
	SceneHandler *m_previous = nullptr;
};

class SceneHandler : public IEventReceiver {
public:
	virtual ~SceneHandler() {}

	void init(Render *render) { m_render = render; }

	virtual SceneHandlerType runPre(float dtime) = 0;
	virtual void runPost() {}

protected:
	SceneHandler() = default;

	Render *m_render = nullptr;
};
