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

	void OnOpen() override;
	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	std::string world_id = "dummyworld";
	std::string title, code;

private:
	void updateWorldList();
	bool m_dirty_worldlist = false;
	std::vector<std::string> m_public_index_to_worldid,
		m_my_index_to_worldid;
	gui::IGUIListBox *m_publiclist = nullptr;
	gui::IGUIListBox *m_mylist = nullptr;
	gui::IGUIButton *m_refreshbtn = nullptr;
};
