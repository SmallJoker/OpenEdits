#include "guilayout_irrlicht.h"
#include <IGUIElement.h>
#include <IGUITabControl.h>
#include <IVideoDriver.h>

#if 1
	#define PRINT_DBG(...) printf(__VA_ARGS__)
#else
	#define PRINT_DBG(...) do {} while(0)
#endif


namespace guilayout {

IGUIElementWrapper::IGUIElementWrapper(gui::IGUIElement *elem) :
	m_element(elem)
{
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
}

void IGUIElementWrapper::updatePosition()
{
	core::recti rect {
		pos[DIR_LEFT],
		pos[DIR_UP],
		pos[DIR_RIGHT],
		pos[DIR_DOWN]
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

void IGUIElementWrapper::debugFillArea(irr::video::IVideoDriver *driver, uint32_t color)
{
	color &= 0x00FFFFFF;

	if (m_element) {
		core::recti rect = m_element->getAbsoluteClippingRect();
		driver->draw2DLine(
			rect.UpperLeftCorner, rect.LowerRightCorner,
			0xFF000000 | color
		);
	}

	auto drawfunc = [driver, &color] (Element *e) -> bool {
		auto ptr = dynamic_cast<IGUIElementWrapper *>(e);
		if (!ptr || !ptr->m_element)
			return true;
		if (!ptr->m_element->isTrulyVisible())
			return false;

		color = 0xFF000000 | (color << 4) | (color >> 16);
		core::recti rect = ptr->m_element->getAbsoluteClippingRect();
		driver->draw2DLine(
			rect.UpperLeftCorner, rect.LowerRightCorner,
			0xFF000000 | color
		);
		return true;
	};

	for (auto &e : m_children) {
		e->doRecursive(drawfunc);
	}
}

}
