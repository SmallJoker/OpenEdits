#include "blockselector.h"

#include "core/blockmanager.h"
#include "core/world.h" // BlockUpdate
#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIImage.h>
#include <IGUITabControl.h>
#include <ITexture.h>
#include <IVideoDriver.h>

enum ElementId : int {
	ID_HOTBAR_0 = 300, // Counted by button number
	ID_SHOWMORE = 350, // [+] [-] button
	ID_TabControl,
	ID_BoxCoinDoor,

	ID_SELECTOR_TAB_0, // Offset for BlockDrawType tabs
	ID_SELECTOR_0 = 400, // Offset for the block ID
	ID_SELECTOR_MAX = ID_SELECTOR_0 + 1200,
};

static const core::dimension2di BTN_SIZE(30, 30);

SceneBlockSelector::SceneBlockSelector(gui::IGUIEnvironment *gui)
{
	m_gui = gui;
	m_hotbar_ids = { 1, 2, 3, 4, 9, 10, 48 };
}

SceneBlockSelector::~SceneBlockSelector()
{
}

void SceneBlockSelector::draw()
{
	core::recti rect(
		m_hotbar_pos,
		BTN_SIZE
	);

	for (size_t i = 0; i < m_hotbar_ids.size(); ++i) {
		drawBlockButton(m_hotbar_ids[i], rect, nullptr, ID_HOTBAR_0 + i);

		rect += core::vector2di(BTN_SIZE.Width, 0);
	}

	// Nice small gap
	rect += core::vector2di(5, 0);
	m_showmore = m_gui->addButton(rect, nullptr, ID_SHOWMORE);
	drawBlockSelector();

	m_highlight = m_gui->addImage(rect);
	// Texture size must match BTN_SIZE to display properly
	m_highlight->setImage(m_gui->getVideoDriver()->getTexture("assets/textures/block_selected.png"));
	selectBlockId(m_selected_bid, false);
}

void SceneBlockSelector::step(float dtime)
{
}

bool SceneBlockSelector::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_MOUSE_INPUT_EVENT) {
		switch (e.MouseInput.Event) {
			case EMIE_LMOUSE_PRESSED_DOWN:
			{
				// Avoid click-through
				core::vector2di pos(e.MouseInput.X, e.MouseInput.Y);
				auto element = m_showmore->getElementFromPoint(pos);
				s32 id = element ? element->getID() : 0;
				if (id >= ID_SELECTOR_0 && id < ID_SELECTOR_MAX) {
					m_dragged_bid = id - ID_SELECTOR_0;
				}
			}
			break;
			case EMIE_LMOUSE_LEFT_UP:
			{
				if (m_dragged_bid == Block::ID_INVALID)
					break;
				bid_t dragged_bid = m_dragged_bid;
				m_dragged_bid = Block::ID_INVALID;

				// Copy to hotbar
				core::vector2di pos(e.MouseInput.X, e.MouseInput.Y);
				auto root = m_gui->getRootGUIElement();
				auto element = root->getElementFromPoint(pos);
				s32 id = element ? element->getID() : 0;
				if (id >= ID_HOTBAR_0 && id < ID_HOTBAR_0 + (int)m_hotbar_ids.size()) {
					m_hotbar_ids.at(id - ID_HOTBAR_0) = dragged_bid;

					// Update button image
					auto rect = element->getAbsoluteClippingRect();
					root->removeChild(element);
					drawBlockButton(dragged_bid, rect, nullptr, id);
					return true;
				}
			}
			break;
			default: break;
		}

		core::vector2di pos(e.MouseInput.X, e.MouseInput.Y);
		auto element = m_showmore->getElementFromPoint(pos);
		auto focused = m_gui->getFocus();
		if (!element && focused && focused->getID() == ID_BoxCoinDoor) {
			switch (e.MouseInput.Event) {
			case EMIE_LMOUSE_PRESSED_DOWN:
			case EMIE_RMOUSE_PRESSED_DOWN:
			case EMIE_MMOUSE_PRESSED_DOWN:
				m_gui->setFocus(nullptr);
				break;
			default: break;
			}
		}
	}
	if (e.EventType == EET_GUI_EVENT && e.GUIEvent.EventType == gui::EGET_EDITBOX_CHANGED) {
		int id = e.GUIEvent.Caller->getID();
		if (id == ID_BoxCoinDoor) {
			auto box = (gui::IGUIEditBox *)e.GUIEvent.Caller;
			auto wtext = box->getText();
			int val = -1;
			int n = swscanf(wtext, L"%d", &val);
			if (n == 1 && val >= 0 && val <= 127) {
				m_selected_param1 = val;
				box->setOverrideColor(0xFF000000);
			} else {
				// red highlight
				box->setOverrideColor(0xFFCC0000);
			}
		}
	}
	if (e.EventType == EET_GUI_EVENT && e.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
		auto btn = e.GUIEvent.Caller;
		int id = btn->getID();
		// Select one from the hotbar
		if (selectBlockId(id, true)) {
			m_gui->setFocus(nullptr);
			return true;
		}
		if (id >= ID_SELECTOR_0 && id < ID_SELECTOR_MAX) {
			// Almost the same as with the hotbar
			if (!selectBlockId(id - ID_SELECTOR_0, false))
				return false;

			auto elem = m_showmore->getElementFromId(ID_BoxCoinDoor, true);
			if (!elem && (m_selected_bid == Block::ID_COINDOOR || m_selected_bid == Block::ID_COINGATE)) {
				core::recti rect(
					core::vector2di(-BTN_SIZE.Width * 0.5f, BTN_SIZE.Height + 2),
					core::dimension2di(BTN_SIZE.Width * 2, 35)
				);

				wchar_t buf[10];
				swprintf(buf, 10, L"%d", (int)m_selected_param1);
				auto element = m_gui->addEditBox(buf, rect, true, btn, ID_BoxCoinDoor);
				element->setNotClipped(true);
				element->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);
				m_gui->setFocus(element);
			} else if (elem) {
				elem->remove();
			}

			m_gui->setFocus(nullptr);
			return true;
		}
		// Show/hide block selector
		if (id == ID_SHOWMORE) {
			toggleShowMore();
			m_gui->setFocus(nullptr);
			return true;
		}
	}
	if (e.EventType == EET_GUI_EVENT && e.GUIEvent.EventType == gui::EGET_TAB_CHANGED) {
		auto element = (gui::IGUITabControl *)e.GUIEvent.Caller;
		m_last_selected_tab = element->getActiveTab();
	}
	if (e.EventType == EET_KEY_INPUT_EVENT) {
		{
			auto element = m_gui->getFocus();
			if (element && element->getType() == gui::EGUIET_EDIT_BOX)
				return false;
		}

		if (!e.KeyInput.PressedDown || e.KeyInput.Shift)
			return false;

		switch (e.KeyInput.Key) {
			case KEY_KEY_E:
				toggleShowMore();
				return true;
			case KEY_KEY_1:
			case KEY_KEY_2:
			case KEY_KEY_3:
			case KEY_KEY_4:
			case KEY_KEY_5:
			case KEY_KEY_6:
			case KEY_KEY_7:
			case KEY_KEY_8:
			case KEY_KEY_9:
				{
					int id = e.KeyInput.Key - KEY_KEY_1 + ID_HOTBAR_0;
					if (selectBlockId(id, true))
						return true;
				}
				break;
			default: break;
		}
	}
	return false;
}

void SceneBlockSelector::toggleShowMore()
{
	m_show_selector ^= true;
	drawBlockSelector();
}

void SceneBlockSelector::getBlockUpdate(BlockUpdate &bu)
{
	bu.set(m_selected_bid);

	using T = BlockParams::Type;
	switch (bu.params.getType()) {
		case T::Gate:
			bu.params.gate.value = m_selected_param1;
			break;
		default:
			break;
	}
}


bool SceneBlockSelector::drawBlockButton(bid_t bid, const core::recti &rect, gui::IGUIElement *parent, int id)
{
	auto e = m_gui->addButton(rect, parent, id);

	auto props = g_blockmanager->getProps(bid);
	if (props) {
		BlockTile tile = props->tiles[0];

		auto dim = tile.texture->getOriginalSize();
		core::recti rect(
			core::vector2di(dim.Height * tile.texture_offset, 0),
			core::dimension2di(dim.Height, dim.Height)
		);
		e->setImage(tile.texture, rect);
	} else {
		e->setText(L"E");
	}
	return !!props;
}


void SceneBlockSelector::drawBlockSelector()
{
	m_showmore->removeAllChildren();
	if (!m_show_selector) {
		m_showmore->setText(L"+");
		return;
	}
	m_showmore->setText(L"-");

	static const core::vector2di SPACING(7, 7);
	struct TabData {
		const wchar_t *name;
		gui::IGUIElement *tab = nullptr;
		core::vector2di pos;
	} tabs_data[(int)BlockDrawType::Invalid] = {
		{ L"Solid" },
		{ L"Action" },
		{ L"Decoration" },
		{ L"Background" },
	};

	const int offset_x = m_showmore->getAbsolutePosition().UpperLeftCorner.X;
	const core::dimension2di content_size(550, 150);
	core::recti rect_tab(
		core::vector2di(10 - offset_x, -150 - 5),
		content_size
	);

	// Prepare to add tabs
	auto skin = m_gui->getSkin();
	video::SColor color(skin->getColor(gui::EGDC_3D_HIGH_LIGHT));

	auto tc = m_gui->addTabControl(rect_tab, m_showmore);
	tc->setID(ID_TabControl);
	tc->setTabHeight(30);
	tc->setNotClipped(true);

	// Add category tabs
	for (TabData &td : tabs_data) {
		auto e = tc->addTab(td.name, (&td - tabs_data) + ID_SELECTOR_TAB_0);
		e->setBackgroundColor(color);
		e->setDrawBackground(true);
		td.tab = e;
		td.pos = SPACING;
	}
	tc->setActiveTab(m_last_selected_tab);

	// Iterate through packs
	auto &packs = g_blockmanager->getPacks();
	for (auto pack : packs) {
		if (pack->default_type == BlockDrawType::Invalid)
			continue;

		TabData &td = tabs_data[(int)pack->default_type];
		int required_width = SPACING.X + BTN_SIZE.Width * pack->block_ids.size();
		if (td.pos.X + required_width  > content_size.Width) {
			// does not fit into the current row, move to the next one
			td.pos.X = SPACING.X;
			td.pos.Y += BTN_SIZE.Height + SPACING.Y;
		}

		core::recti rect_b(td.pos, BTN_SIZE);
		for (bid_t bid : pack->block_ids) {
			drawBlockButton(bid, rect_b, td.tab, ID_SELECTOR_0 + (int)bid);
			rect_b += core::vector2di(BTN_SIZE.Width, 0);
		}
		td.pos.X += required_width;
	}
}

bool SceneBlockSelector::selectBlockId(int what, bool is_element_id)
{
	bid_t selected;
	s32 hotbar_index = -1;
	if (is_element_id) {
		// Get block ID from element ID
		if (what < ID_HOTBAR_0 || what >= ID_HOTBAR_0 + (int)m_hotbar_ids.size())
			return false;

		hotbar_index = what - ID_HOTBAR_0;
		selected = m_hotbar_ids.at(hotbar_index);
	} else {
		// Try to get element ID based on block ID
		selected = what;
		for (size_t i = 0; i < m_hotbar_ids.size(); ++i) {
			if (m_hotbar_ids[i] == selected) {
				hotbar_index = i;
				break;
			}
		}
	}

	if (hotbar_index >= 0) {
		m_highlight->setVisible(true);
		m_highlight->setRelativePosition(
			m_hotbar_pos + core::vector2di(BTN_SIZE.Width * hotbar_index, 0)
		);

	} else {
		m_highlight->setVisible(false);
	}

	m_selected_bid = selected;
	return true;
}
