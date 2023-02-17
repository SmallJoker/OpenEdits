#pragma once

#include <dimension2d.h>

namespace irr {
	namespace gui {
		class IGUIImage;
	}
	namespace video {
		class IImage;
		class ITexture;
	}
}

using namespace irr;

class Client;
class Gui;
class SceneGameplay;

class SceneMinimap {
public:
	SceneMinimap(SceneGameplay *parent, Gui *gui);
	~SceneMinimap();

	void draw();
	void step(float dtime);

	void markDirty() { m_is_dirty = true; }
	void toggleVisibility();

private:
	SceneGameplay *m_gameplay = nullptr;
	Gui *m_gui = nullptr;

	video::ITexture *m_texture = nullptr;
	core::dimension2du m_imgsize;

	bool m_is_dirty = true;
	bool m_is_visible = true;
};
