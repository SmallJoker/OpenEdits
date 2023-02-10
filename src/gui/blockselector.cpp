#include "blockselector.h"

#include "core/blockmanager.h"
#include <IGUIButton.h>
#include <IGUIEnvironment.h>
#include <ITexture.h>

static const int ID_HOTBAR_0 = 300;

SceneBlockSelector::SceneBlockSelector(gui::IGUIEnvironment *gui)
{
	m_gui = gui;
	m_hotbar_ids = { 0, 1, 2, 3, 4, 10, 11 };
}

void SceneBlockSelector::draw()
{
	static const core::dimension2di BTN_SIZE(30, 30);
	core::recti rect(
		m_hotbar_pos,
		BTN_SIZE
	);

	for (size_t i = 0; i < m_hotbar_ids.size(); ++i) {
		const bid_t bid = m_hotbar_ids[i];

		auto e = m_gui->addButton(rect, nullptr, ID_HOTBAR_0 + i);

		auto props = g_blockmanager->getProps(bid);
		if (props) {
			int height = props->texture->getOriginalSize().Height;
			core::recti source(
				core::vector2di(height * props->texture_offset, 0),
				core::dimension2di(height, height)
			);
			e->setImage(props->texture, source);
		} else {
			e->setText(L"E");
		}

		rect += core::vector2di(BTN_SIZE.Width, 0);
	}
}

void SceneBlockSelector::step(float dtime)
{
}

bool SceneBlockSelector::OnEvent(const SEvent &e)
{
	if (e.EventType != EET_GUI_EVENT)
		return false;

	if (e.GUIEvent.EventType != gui::EGET_BUTTON_CLICKED)
		return false;

	int id = e.GUIEvent.Caller->getID();
	if (id < ID_HOTBAR_0 || id >= ID_HOTBAR_0 + m_hotbar_ids.size())
		return false;

	m_selected_bid = m_hotbar_ids.at(id - ID_HOTBAR_0);
	printf("Select bid_t=%d\n", m_selected_bid);

	m_gui->setFocus(nullptr);
	return true;
}

