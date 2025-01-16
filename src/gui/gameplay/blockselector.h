#pragma once

#include "core/types.h"
#include <IEventReceiver.h>
#include <rect.h>
#include <vector>

namespace irr {
	namespace gui {
		class IGUIEditBox;
		class IGUIElement;
		class IGUIEnvironment;
		class IGUIImage;
	}
}

struct BlockParams;
struct BlockUpdate;
class SceneGameplay;

class SceneBlockSelector : public IEventReceiver {
public:
	SceneBlockSelector(SceneGameplay *parent, gui::IGUIEnvironment *gui);
	~SceneBlockSelector();

	void setHotbarPos(const core::position2di &pos) { m_hotbar_pos = pos; }
	void setEnabled(bool enabled) { m_do_enable = enabled; }

	void draw();
	bool OnEvent(const SEvent &e) override;
	void toggleShowMore();
	void setEraseMode(bool erase);

	void setParamsFromBlock(bid_t block_id, BlockParams &params);
	void getBlockUpdate(BlockUpdate &bu);

private:
	gui::IGUIEditBox *createInputBox(const SEvent &e, s32 id, bool may_open);

	/// `e` for caller information
	void toggleScriptElements(const SEvent &e);

	void toggleCoinBox(const SEvent &e);
	void readCoinBoxValue(const SEvent &e);

	void toggleNoteBox(const SEvent &e);
	void readNoteBoxValue(const SEvent &e);

	void toggleTeleporterBox(const SEvent &e);
	void readTeleporterBox();

	void toggleTextBox(const SEvent &e);
	void readTextBoxValue(const SEvent &e);

	bool drawBlockButton(bid_t bid, const core::recti &rect, gui::IGUIElement *parent, int id);
	void drawBlockSelector();
	bool selectBlockId(int what, bool is_element_id);

	SceneGameplay *m_gameplay = nullptr;
	gui::IGUIEnvironment *m_gui = nullptr;

	bool m_do_enable = false; // Two-staged to update in draw()
	bool m_enabled = false;

	std::vector<bid_t> m_hotbar_ids;
	core::position2di m_hotbar_pos;
	bid_t m_selected_bid = 0;
	bid_t m_last_selected_bid = 0; // when holding shift
	u8 m_selected_param1 = 0;
	u8 m_selected_note = 0;
	u8 m_selected_tp_id = 0;
	u8 m_selected_tp_dst = 0;
	std::string m_selected_text = "Hello World";

	bool m_show_selector = false;
	bid_t m_dragged_bid = Block::ID_INVALID;
	gui::IGUIElement *m_showmore = nullptr;
	gui::IGUIImage *m_highlight = nullptr;

	int m_last_selected_tab = 0;
};
