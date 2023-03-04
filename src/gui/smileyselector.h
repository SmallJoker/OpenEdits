#pragma once

#include "core/types.h"
#include <IEventReceiver.h>
#include <rect.h>
#include <vector>

namespace irr {
	namespace gui {
		class IGUIElement;
		class IGUIEnvironment;
		class IGUIImage;
	}
	namespace video {
		class ITexture;
	}
}

class Gui;

class SceneSmileySelector : public IEventReceiver {
public:
	SceneSmileySelector(Gui *gui);
	~SceneSmileySelector();

	void setSelectorPos(const core::position2di &pos) { m_selector_pos = pos; }

	void draw();
	bool OnEvent(const SEvent &e) override;
	void toggleSelector();

private:
	void drawSelector();
	void selectSmiley(int smiley_id);

	Gui *m_gui = nullptr;
	video::ITexture *m_texture = nullptr;

	core::position2di m_selector_pos;

	bool m_show_selector = false;
	gui::IGUIElement *m_button = nullptr;
};
