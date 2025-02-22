#pragma once

#include "gui.h"
#include "core/friends.h"
#include <string>

namespace irr {
	namespace gui {
		class IGUIButton;
		class IGUIEditBox;
		class IGUIListBox;
		class IGUITabControl;
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
	void addWorldsTab(gui::IGUITabControl *tc);
	void addFriendsTab(gui::IGUITabControl *tc);
	void addSearchTab(gui::IGUITabControl *tc);

	void updateWorldList();
	bool m_dirty_worldlist = false;

	std::vector<std::string>
		m_public_index_to_worldid,
		m_my_index_to_worldid,
		m_import_index_to_worldid;
	gui::IGUIListBox *m_publiclist = nullptr;
	gui::IGUIListBox *m_mylist = nullptr;
	gui::IGUIListBox *m_importlist = nullptr;
	gui::IGUIButton *m_refreshbtn = nullptr;

	// Called by updateWorldList
	void updateFriendsList();

	struct FriendsList {
		std::vector<LobbyFriend> index_LUT;

		gui::IGUIListBox *list = nullptr;
	} m_friends;

	void updateSearchList();
	bool m_dirty_search = false;
};
