#pragma once

#include "gui.h"
#include <string>

namespace irr {
	namespace gui {
		class IGUIButton;
		class IGUIEditBox;
		class IGUIListBox;
	}
}

class SceneLobby : public SceneHandler {
public:
	SceneLobby();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	std::string world_id = "dummyworld";

private:
	void updateWorldList();
	bool m_dirty_worldlist = false;
	std::vector<std::string> m_index_to_worldid;
	gui::IGUIListBox *m_worldlist = nullptr;
	gui::IGUIButton *m_refreshbtn = nullptr;
};
