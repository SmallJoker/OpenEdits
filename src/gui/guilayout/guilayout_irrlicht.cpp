#include "guilayout_irrlicht.h"
#include <IGUIElement.h>
#include <IGUIStaticText.h>
#include <IGUITabControl.h>
#include <IVideoDriver.h>

#if 0
	#define PRINT_DBG(...) printf(__VA_ARGS__)
#else
	#define PRINT_DBG(...) do {} while(0)
#endif


namespace guilayout {

IGUIElementWrapper::IGUIElementWrapper(gui::IGUIElement *elem)
{
	setElement(elem);
	if (!m_element) {
		PRINT_DBG("+ IGUIElement (nullptr)\n");
		return;
	}
	core::stringc dst;
	core::wStringToUTF8(dst, m_element->getTypeName());
	PRINT_DBG("+ IGUIElement %s\n", dst.c_str());
}

IGUIElementWrapper::~IGUIElementWrapper()
{
	if (!m_element) {
		PRINT_DBG("- IGUIElement (nullptr)\n");
		return;
	}

	// When created with the GUI environment root element,
	// ->drop() and ->remove() are not needed because
	// Irrlicht automatically frees its children.

	core::stringc dst;
	core::wStringToUTF8(dst, m_element->getTypeName());
	PRINT_DBG("- IGUIElement %s\n", dst.c_str());

	m_element->drop();
}

void IGUIElementWrapper::setElement(gui::IGUIElement *elem)
{
	if (m_element)
		m_element->drop();

	elem->grab();
	m_element = elem;

	gui::EGUI_ELEMENT_TYPE type = m_element->getType();

	switch (type) {
		case gui::EGUIET_BUTTON:
		case gui::EGUIET_EDIT_BOX:
			margin = { 1, 1, 1, 1 };
			expand = { 5, 1 };
			setTextlike(type == gui::EGUIET_BUTTON);
			break;
		case gui::EGUIET_STATIC_TEXT:
			margin = { 1, 1, 1, 0 }; // left align
			expand = { 0, 0 }; // no benefit from larger container
			setTextlike(true);
			{
				gui::IGUIStaticText *e = (gui::IGUIStaticText *)m_element;
				e->setTextAlignment(gui::EGUIA_UPPERLEFT, gui::EGUIA_CENTER);
			}
			break;
		default: break;
	}
}

void IGUIElementWrapper::updatePosition()
{
	core::recti rect {
		pos[DIR_LEFT],
		pos[DIR_UP],
		pos[DIR_RIGHT] - 1,
		pos[DIR_DOWN] - 1
	};

#if 0
	core::stringc dst;
	core::wStringToUTF8(dst, element->getTypeName());
	printf("Draw element %s: X % 3i, Y % 3i | W % 3i, H % 3i\n",
		dst.c_str(),
		rect.UpperLeftCorner.X,
		rect.UpperLeftCorner.Y,
		rect.getWidth(),
		rect.getHeight()
	);
#endif

	m_element->setRelativePosition(rect);

	// Deal nested elements
	switch (m_element->getType()) {
	case gui::EGUIET_TAB_CONTROL:
		{
			auto tc = (gui::IGUITabControl *)m_element;
			size_t count = std::min<size_t>(tc->getTabCount(), m_children.size());
			for (size_t i = 0; i < count; ++i) {
				core::recti crect = tc->getTab(i)->getAbsoluteClippingRect();
				m_children[i]->start(
					{0, 0},
					{(u16)crect.getWidth(), (u16)crect.getHeight()}
				);
			}
		}
		break;
	default:
		break;
	}
}

void IGUIElementWrapper::doRecursive(std::function<bool(Element *)> callback)
{
	if (!callback(this))
		return;

	for (auto &e : m_children) {
		e->doRecursive(callback);
	}
}

void IGUIElementWrapper::debugFillArea(Element *e, video::IVideoDriver *driver, uint32_t color)
{
	color &= 0x00FFFFFF;

	auto drawfunc = [driver, &color] (Element *e) -> bool {
		auto ptr = dynamic_cast<IGUIElementWrapper *>(e);
		if (!ptr || !ptr->m_element)
			return true; // maybe a child element is drawable ...?
		if (!ptr->m_element->isTrulyVisible())
			return false;

		// semi-transparent
		color = (color & 0x00FFFFFF) | 0x55000000;
		core::recti rect = ptr->m_element->getAbsoluteClippingRect();
		driver->draw2DRectangle(color, rect);

		// Show minimal rect
		core::vector2di center = (rect.UpperLeftCorner + rect.LowerRightCorner) / 2;
		core::recti minr(
			center.X - e->min_size[SIZE_X] / 2,
			center.Y - e->min_size[SIZE_Y] / 2,
			center.X + e->min_size[SIZE_X] / 2,
			center.Y + e->min_size[SIZE_Y] / 2
		);
		driver->draw2DRectangle(color + 0x22000000, minr);

		// randomize for next element
		color = ((color >> 1) ^ 0x13579B) + 0xB97531;
		return true;
	};

	e->doRecursive(drawfunc);
}

void IGUIElementWrapper::setTextlike(bool use_get_text)
{
	// TODO: getSkin()->getFont()->getDimension() for a more accurate number
	constexpr u16 TEXT_HEIGHT = 20;

	float len = 6;
	if (use_get_text)
		len = std::max<u16>(len, wcslen(m_element->getText()));

	min_size = { (u16)(len * TEXT_HEIGHT * 0.6f), TEXT_HEIGHT + 4 };
}

}
