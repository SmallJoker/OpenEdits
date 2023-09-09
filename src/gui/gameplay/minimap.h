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
	void updateMap();
	void updatePlayers(float dtime);

	SceneGameplay *m_gameplay = nullptr;
	Gui *m_gui = nullptr;

	video::ITexture *m_texture = nullptr;
	float m_overlay_timer = 0;
	video::IImage *m_overlay_img = nullptr;
	gui::IGUIImage *m_overlay_elm = nullptr;
	video::ITexture *m_overlay_txt = nullptr; // players
	core::dimension2du m_imgsize;

	bool m_is_dirty = true;
	bool m_is_visible = false;
	bool m_need_force_reload = false;
};
