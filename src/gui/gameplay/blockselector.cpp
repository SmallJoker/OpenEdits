#include "blockselector.h"
#include "gameplay.h"

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
	ID_SHOWMORE = 320, // [+] [-] button
	ID_TabControl,
	ID_BoxCoinDoor,
	ID_BoxNote,
	ID_TabTeleporter,
	ID_TabTeleporter_ID,
	ID_TabTeleporter_DST,
	ID_BoxText,

	ID_SELECTOR_TAB_0, // Offset for BlockDrawType tabs
	ID_SELECTOR_0 = 400, // Offset for the block ID
	ID_SELECTOR_MAX = ID_SELECTOR_0 + 1200,
};

static const core::dimension2di BTN_SIZE(30, 30);

SceneBlockSelector::SceneBlockSelector(SceneGameplay *parent, gui::IGUIEnvironment *gui)
{
	m_gameplay = parent;
	m_gui = gui;
	m_hotbar_ids = { 0, 9, 10, 2, 4, 48, 46, 67 };
}

SceneBlockSelector::~SceneBlockSelector()
{
}

void SceneBlockSelector::draw()
{
	m_enabled = m_do_enable;
	if (!m_enabled)
		return;

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
	m_highlight->setImage(m_gui->getVideoDriver()->getTexture("assets/textures/selected_30px.png"));
	m_highlight->setScaleImage(true);
	selectBlockId(m_selected_bid, false);
}

static void editbox_move_to_end(gui::IGUIElement *element)
{
	SEvent ev;
	memset(&ev, 0, sizeof(ev));
	ev.EventType = EET_KEY_INPUT_EVENT;
	ev.KeyInput.PressedDown = true;
	ev.KeyInput.Char = '\0';
	ev.KeyInput.Key = KEY_END;
	element->OnEvent(ev);
}

gui::IGUIEditBox *SceneBlockSelector::createInputBox(const SEvent &e, s32 id, bool may_open)
{
	auto elem = m_showmore->getElementFromId(id, true);
	if (!may_open || elem) {
		if (elem)
			elem->remove();
		return nullptr;
	}

	core::recti rect(
		core::vector2di(-BTN_SIZE.Width * 0.5f, BTN_SIZE.Height + 2),
		core::dimension2di(BTN_SIZE.Width * 2, 30)
	);

	auto element = m_gui->addEditBox(L"", rect, true, e.GUIEvent.Caller, id);
	element->setNotClipped(true);
	element->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);
	m_gui->setFocus(element);

	return element;
}

void SceneBlockSelector::toggleCoinBox(const SEvent &e)
{
	bool may_open = m_selected_bid == Block::ID_COINDOOR || m_selected_bid == Block::ID_COINGATE;
	auto element = createInputBox(e, ID_BoxCoinDoor, may_open);
	if (!element)
		return;

	wchar_t buf[10];
	swprintf(buf, 10, L"%d", (int)m_selected_param1);
	element->setText(buf);
	editbox_move_to_end(element);
}

static bool sanitize_input(gui::IGUIEditBox *box, int *val, int min, int max)
{
	auto wtext = box->getText();
	int n = swscanf(wtext, L"%d", val);
	// Indicate bad numeric input
	if (n == 1 && *val >= min && *val <= max) {
		box->setOverrideColor(0xFF000000);
		return true;
	}

	// red highlight
	box->setOverrideColor(0xFFCC0000);
	return false;
}

void SceneBlockSelector::readCoinBoxValue(const SEvent &e)
{
	auto box = (gui::IGUIEditBox *)e.GUIEvent.Caller;
	int val = -1;
	if (sanitize_input(box, &val, 0, 127))
		m_selected_param1 = val;
}

void SceneBlockSelector::toggleNoteBox(const SEvent &e)
{
	bool may_open = m_selected_bid == Block::ID_PIANO;
	auto element = createInputBox(e, ID_BoxNote, may_open);
	if (!element)
		return;

	// TODO: Change to a more readable format
	wchar_t buf[10];
	swprintf(buf, 10, L"%d", (int)m_selected_note);
	element->setText(buf);
	editbox_move_to_end(element);
}

void SceneBlockSelector::readNoteBoxValue(const SEvent &e)
{
	auto box = (gui::IGUIEditBox *)e.GUIEvent.Caller;
	int val = -1;
	if (sanitize_input(box, &val, 0, 12 * 4))
		m_selected_note = val;
}

void SceneBlockSelector::toggleTeleporterBox(const SEvent &e)
{
	auto elem = m_showmore->getElementFromId(ID_TabTeleporter, true);
	bool may_open = m_selected_bid == Block::ID_TELEPORTER;
	if (!may_open || elem) {
		if (elem)
			elem->remove();
		return;
	}

	core::recti rect_tab(
		core::vector2di(-BTN_SIZE.Width * 1.5f, BTN_SIZE.Height + 2),
		core::dimension2di(BTN_SIZE.Width * 2 + 42, 70)
	);

	auto skin = m_gui->getSkin();
	video::SColor color(skin->getColor(gui::EGDC_3D_FACE));

	auto tab = m_gui->addTab(rect_tab, e.GUIEvent.Caller, ID_TabTeleporter);
	tab->setBackgroundColor(color);
	tab->setDrawBackground(true);
	tab->setNotClipped(true);

	// Add labels and inputs
	core::recti rect_label(
		core::vector2di(3, 9),
		core::dimension2di(40, 30)
	);
	core::recti rect_input(
		core::vector2di(40, 3),
		core::dimension2di(BTN_SIZE.Width * 2, 30)
	);

	{
		// ID
		m_gui->addStaticText(L"ID", rect_label, false, false, tab);
		wchar_t buf[10];
		swprintf(buf, 10, L"%d", (int)m_selected_tp_id);
		auto inp = m_gui->addEditBox(buf, rect_input, true, tab, ID_TabTeleporter_ID);
		inp->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);
	}
	core::vector2di spacing(0, rect_input.getHeight() + 2);
	rect_label += spacing;
	rect_input += spacing;
	{
		// DST
		m_gui->addStaticText(L"DST", rect_label, false, false, tab);
		wchar_t buf[10];
		swprintf(buf, 10, L"%d", (int)m_selected_tp_dst);
		auto inp = m_gui->addEditBox(buf, rect_input, true, tab, ID_TabTeleporter_DST);
		inp->setTextAlignment(gui::EGUIA_CENTER, gui::EGUIA_CENTER);
	}
}

void SceneBlockSelector::readTeleporterBox()
{
	auto inp_id = (gui::IGUIEditBox *)m_showmore->getElementFromId(ID_TabTeleporter_ID, true);
	auto inp_dst = (gui::IGUIEditBox *)m_showmore->getElementFromId(ID_TabTeleporter_DST, true);
	if (!inp_id || !inp_dst)
		return;

	int val = -1;
	if (sanitize_input(inp_id, &val, 0, 255))
		m_selected_tp_id = val;
	if (sanitize_input(inp_dst, &val, 0, 255))
		m_selected_tp_dst = val;
}

void SceneBlockSelector::toggleTextBox(const SEvent &e)
{
	auto elem = m_showmore->getElementFromId(ID_BoxText, true);
	bool may_open = m_selected_bid == Block::ID_TEXT;
	if (!may_open || elem) {
		if (elem)
			elem->remove();
		return;
	}

	core::recti rect(
		core::vector2di(-BTN_SIZE.Width, BTN_SIZE.Height + 2),
		core::dimension2di(BTN_SIZE.Width * 4, 30)
	);

	std::wstring wstr;
	utf8_to_wide(wstr, m_selected_text.c_str());
	auto element = m_gui->addEditBox(wstr.c_str(), rect, true, e.GUIEvent.Caller, ID_BoxText);
	element->setNotClipped(true);
	m_gui->setFocus(element);
	editbox_move_to_end(element);
}

void SceneBlockSelector::readTextBoxValue(const SEvent &e)
{
	auto box = (gui::IGUIEditBox *)e.GUIEvent.Caller;
	wide_to_utf8(m_selected_text, box->getText());
}


bool SceneBlockSelector::OnEvent(const SEvent &e)
{
	if (!m_enabled)
		return false;

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

				// Copy dragged block to hotbar
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

					// Make it so that the selector texture is always above
					root->bringToFront(m_highlight);
					return true;
				}
			}
			break;
			default: break;
		}
	}
	if (e.EventType == EET_GUI_EVENT && e.GUIEvent.EventType == gui::EGET_EDITBOX_CHANGED) {
		int id = e.GUIEvent.Caller->getID();
		switch (id) {
			case ID_BoxCoinDoor:
				readCoinBoxValue(e);
				break;
			case ID_BoxNote:
				readNoteBoxValue(e);
				break;
			case ID_TabTeleporter_ID:
			case ID_TabTeleporter_DST:
				readTeleporterBox();
				break;
			case ID_BoxText:
				readTextBoxValue(e);
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

			toggleCoinBox(e);
			toggleNoteBox(e);
			toggleTeleporterBox(e);
			toggleTextBox(e);
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
			case KEY_TAB:
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

void SceneBlockSelector::setEraseMode(bool erase)
{
	if (erase)
		selectBlockId(0, false);
	else
		selectBlockId(m_last_selected_bid, false);
}

void SceneBlockSelector::setParamsFromBlock(bid_t block_id, BlockParams &params)
{
	if (!g_blockmanager->getProps(block_id))
		return;

	switch (block_id) {
		case Block::ID_COINDOOR:
		case Block::ID_COINGATE:
			if (params.getType() != BlockParams::Type::U8)
				return;

			m_selected_param1 = params.param_u8;
			break;
		case Block::ID_PIANO:
			if (params.getType() != BlockParams::Type::U8)
				return;

			m_selected_note = params.param_u8;
			break;
		case Block::ID_TELEPORTER:
			if (params.getType() != BlockParams::Type::Teleporter)
				return;

			m_selected_tp_id = params.teleporter.id;
			m_selected_tp_dst = params.teleporter.dst_id;
			break;
		case Block::ID_TEXT:
			if (params.getType() != BlockParams::Type::Text)
				return;

			m_selected_text = *params.text;
			break;
		default:
			if (params.getType() != BlockParams::Type::None)
				return;
			break; // select block
	}

	selectBlockId(block_id, false);
}

void SceneBlockSelector::getBlockUpdate(BlockUpdate &bu)
{
	bu.set(m_selected_bid);

	switch (bu.getId()) {
		case Block::ID_COINDOOR:
		case Block::ID_COINGATE:
			bu.params.param_u8 = m_selected_param1;
			break;
		case Block::ID_PIANO:
			bu.params.param_u8 = m_selected_note;
			break;
		case Block::ID_TELEPORTER:
			bu.params.teleporter.id = m_selected_tp_id;
			bu.params.teleporter.dst_id = m_selected_tp_dst;
			break;
		case Block::ID_TEXT:
			*bu.params.text = m_selected_text;
		default:
			break;
	}
}

static video::ITexture *make_pressed_image(video::IVideoDriver *driver, video::ITexture *source)
{
	static std::map<video::ITexture *, video::ITexture *> cache;

	auto it = cache.find(source);
	if (it != cache.end())
		return it->second;

	auto dim = source->getOriginalSize();
	video::IImage *img = driver->createImage(source,
		core::vector2di(),
		core::dimension2du(dim.Height, dim.Height)
	);

	for (u32 y = 0; y < dim.Height; ++y)
	for (u32 x = 0; x < dim.Width; ++x) {
		auto color = img->getPixel(x, y);
		color.setRed(color.getRed() * 0.7f);
		color.setGreen(color.getGreen() * 0.7f);
		color.setBlue(color.getBlue() * 0.7f);
		img->setPixel(x, y, color);
	}

	char buf[255];
	snprintf(buf, sizeof(buf), "%p__pressed", source);

	video::ITexture *out = driver->addTexture(buf, img);
	img->drop();

	cache.emplace(source, out);
	return out;
}

extern BlockManager *g_blockmanager;

bool SceneBlockSelector::drawBlockButton(bid_t bid, const core::recti &rect, gui::IGUIElement *parent, int id)
{
	auto e = m_gui->addButton(rect, parent, id);

	auto props = g_blockmanager->getProps(bid);
	if (props) {
		BlockTile tile = props->tiles[0];

		auto dim = tile.texture->getOriginalSize();
		core::recti rect(
			core::vector2di(0, 0),
			core::dimension2di(dim.Height, dim.Height)
		);
		e->setImage(tile.texture, rect);
		e->setScaleImage(true);
		e->setUseAlphaChannel(false);
		e->setDrawBorder(false);
		e->setPressedImage(make_pressed_image(m_gui->getVideoDriver(), tile.texture));
	} else {
		e->setText(L"E");
	}
	return !!props;
}


void SceneBlockSelector::drawBlockSelector()
{
	if (!m_enabled)
		return;

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

	const core::dimension2di content_size(550, 150);
	const int offset_x = m_showmore->getAbsolutePosition().UpperLeftCorner.X;
	const int content_pos_x = (m_gameplay->getDrawArea().LowerRightCorner.X - content_size.Width) / 2;
	core::recti rect_tab(
		core::vector2di(std::max(content_pos_x, 10) - offset_x, -150 - 5),
		content_size
	);

	// Prepare to add tabs
	auto skin = m_gui->getSkin();
	video::SColor color(skin->getColor(gui::EGDC_3D_SHADOW));

	auto tc = m_gui->addTabControl(rect_tab, m_showmore);
	tc->setID(ID_TabControl);
	tc->setTabHeight(30);
	tc->setNotClipped(true);
	m_gui->getRootGUIElement()->bringToFront(m_showmore);

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
	if (!m_enabled)
		return false;

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

	if (selected != 0)
		m_last_selected_bid = selected;
	m_selected_bid = selected;
	return true;
}
