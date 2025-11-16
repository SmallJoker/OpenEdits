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
class SceneGameplay;

class SceneSmileySelector : public IEventReceiver {
public:
	SceneSmileySelector(SceneGameplay *gameplay);
	~SceneSmileySelector();

	void setSelectorPos(const core::position2di &pos) { m_selector_pos = pos; }

	void draw();
	bool OnEvent(const SEvent &e) override;
	void toggleSelector();

private:
	void drawSelector();
	void selectSmiley(int smiley_id);

	SceneGameplay *m_gameplay = nullptr;
	Gui *m_gui = nullptr;

	core::position2di m_selector_pos;

	bool m_show_selector = false;
	gui::IGUIElement *m_button = nullptr;
};
