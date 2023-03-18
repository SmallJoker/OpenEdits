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
}

struct BlockUpdate;
class SceneGameplay;

class SceneBlockSelector : public IEventReceiver {
public:
	SceneBlockSelector(SceneGameplay *parent, gui::IGUIEnvironment *gui);
	~SceneBlockSelector();

	void setHotbarPos(const core::position2di &pos) { m_hotbar_pos = pos; }
	void setEnabled(bool enabled) { m_enabled = enabled; }

	void draw();
	bool OnEvent(const SEvent &e) override;
	void toggleShowMore();

	void getBlockUpdate(BlockUpdate &bu);

private:
	bool drawBlockButton(bid_t bid, const core::recti &rect, gui::IGUIElement *parent, int id);
	void drawBlockSelector();
	bool selectBlockId(int what, bool is_element_id);

	SceneGameplay *m_gameplay = nullptr;
	gui::IGUIEnvironment *m_gui = nullptr;

	bool m_enabled = false;

	std::vector<bid_t> m_hotbar_ids;
	core::position2di m_hotbar_pos;
	bid_t m_selected_bid = 0;
	u8 m_selected_param1 = 0;

	bool m_show_selector = false;
	bid_t m_dragged_bid = Block::ID_INVALID;
	gui::IGUIElement *m_showmore = nullptr;
	gui::IGUIImage *m_highlight = nullptr;

	int m_last_selected_tab = 0;
};
