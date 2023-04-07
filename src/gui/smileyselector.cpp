#include "smileyselector.h"
#include "gui.h"

#include <IGUIButton.h>
#include <IGUIEditBox.h>
#include <IGUIEnvironment.h>
#include <IGUIImage.h>
#include <IGUITabControl.h>
#include <ITexture.h>
#include <IVideoDriver.h>

enum ElementId : int {
	ID_BUTTON = 3000,
	ID_SELECTOR,
	ID_SMILEY_0 = 3005, // Counted by button number
	ID_SMILEY_MAX = ID_SMILEY_0 + 100
};

static const core::dimension2di BTN_SIZE(35, 30);
static const core::dimension2di PADDING(5, 5);

SceneSmileySelector::SceneSmileySelector(Gui *gui)
{
	m_gui = gui;

	m_texture = gui->guienv->getVideoDriver()->getTexture("assets/textures/smileys.png");
}

SceneSmileySelector::~SceneSmileySelector()
{
}

void SceneSmileySelector::draw()
{
	core::recti rect(
		m_selector_pos,
		BTN_SIZE
	);

	auto gui = m_gui->guienv;

	m_button = gui->addButton(rect, nullptr, ID_BUTTON, L":)");

	drawSelector();
}

bool SceneSmileySelector::OnEvent(const SEvent &e)
{
	if (e.EventType == EET_GUI_EVENT && e.GUIEvent.EventType == gui::EGET_BUTTON_CLICKED) {
		auto btn = e.GUIEvent.Caller;
		int id = btn->getID();
		if (id == ID_BUTTON) {
			toggleSelector();
			m_gui->guienv->setFocus(nullptr);
			return true;
		}

		if (id >= ID_SMILEY_0 && id <= ID_SMILEY_MAX) {
			selectSmiley(id - ID_SMILEY_0);
			toggleSelector();
			return true;
		}
	}
	return false;
}

void SceneSmileySelector::toggleSelector()
{
	m_show_selector ^= true;
	drawSelector();
}

void SceneSmileySelector::drawSelector()
{
	m_button->removeAllChildren();
	if (!m_show_selector) {
		m_button->setText(L":)");
		return;
	}
	m_button->setText(L"-");

	core::vector2di size(4, 0); // Size of the selector (smiley grid)
	auto img_dim = m_texture->getOriginalSize();
	const int total_count = img_dim.Width / img_dim.Height;
	size.Y = std::ceil((float)total_count / size.X);

	core::recti rect_tab(
		core::vector2di(),
		core::dimension2di(
			size.X * BTN_SIZE.Width  + PADDING.Width * 2,
			size.Y * BTN_SIZE.Height + PADDING.Height * 2
		)
	);
	rect_tab -= core::vector2di(rect_tab.getWidth() / 2, rect_tab.getHeight() + 5);

	auto gui = m_gui->guienv;

	auto skin = gui->getSkin();
	video::SColor color(skin->getColor(gui::EGDC_3D_SHADOW));

	auto tab = gui->addTab(rect_tab, m_button, ID_SELECTOR);
	tab->setBackgroundColor(color);
	tab->setDrawBackground(true);
	tab->setNotClipped(true);

	for (int index = 0; index < total_count; ++index) {
		int y = index / size.X;
		int x = index - y * size.X;

		core::recti rect_btn(
			core::vector2di(
				x * BTN_SIZE.Width + PADDING.Width,
				y * BTN_SIZE.Height + PADDING.Height
			),
			BTN_SIZE
		);

		auto e = gui->addButton(rect_btn, tab, ID_SMILEY_0 + index);

		core::recti rect(
			core::vector2di(img_dim.Height * index, 0),
			core::dimension2di(img_dim.Height, img_dim.Height)
		);
		e->setImage(m_texture, rect);
		e->setUseAlphaChannel(true);
	}

}

void SceneSmileySelector::selectSmiley(int smiley_id)
{
	{
		GameEvent e(GameEvent::G2C_SMILEY);
		e.intval = smiley_id;
		m_gui->sendNewEvent(e);
	}
}
