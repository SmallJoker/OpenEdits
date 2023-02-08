#pragma once

#include "client/gameevent.h"
#include "core/macros.h"
#include <IEventReceiver.h>
#include <rect.h>
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
	namespace video {
		class IVideoDriver;
	}
}

using namespace irr;

class Client;
class Server;

class SceneHandler;
class SceneConnect;
class SceneLobby;
class SceneGameplay;

enum class SceneHandlerType {
	Connect,
	Lobby,
	Gameplay,
	CTRL_QUIT,
	CTRL_RENEW
};

void wStringToMultibyte(std::string &dst, const wchar_t *src);

class Gui : public IEventReceiver, public GameEventHandler {
public:
	Gui();
	~Gui();

	void run();

	// Global callbacks
	bool OnEvent(const SEvent &event) override;
	bool OnEvent(GameEvent &e) override;

	void registerHandler(SceneHandlerType type, SceneHandler *handler);

	// Helpers for SceneHandler
	SceneHandler *getHandler(SceneHandlerType type);
	Client *getClient() { return m_client; }

	// Actions to perform
	void connect(SceneConnect *sc);
	void disconnect();
	void joinWorld(SceneLobby *sc);
	void leaveWorld();

	// GUI utility functions
	core::recti getRect(core::vector2df pos_perc, core::dimension2di size);
	void displaceRect(core::recti &rect, core::vector2df pos_perc);

	// For use in SceneHandler
	scene::ISceneManager *scenemgr = nullptr;
	gui::IGUIEnvironment *gui = nullptr;
	gui::IGUIFont *font = nullptr;
	video::IVideoDriver *driver = nullptr; // 2D images
	core::dimension2du window_size;

private:
	inline void setNextScene(SceneHandlerType type) { m_scenetype_next = type; }

	bool m_initialized = false;

	SceneHandlerType m_scenetype_next;
	SceneHandlerType m_scenetype;

	IrrlichtDevice *device = nullptr;

	std::map<SceneHandlerType, SceneHandler *> m_handlers;

	Client *m_client = nullptr;
	Server *m_server = nullptr;
};

class SceneHandler {
public:
	virtual ~SceneHandler() {}
	DISABLE_COPY(SceneHandler)

	void init(Gui *gui) { m_gui = gui; }

	virtual void draw() = 0;
	virtual void step(float dtime) = 0;
	virtual bool OnEvent(const SEvent &e) = 0;
	virtual bool OnEvent(GameEvent &e) = 0;

protected:
	SceneHandler() = default;

	Gui *m_gui = nullptr;
};
