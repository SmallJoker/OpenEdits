#include "guilayout_irrlicht.h"
#include <IGUIElement.h>
#include <IGUIEnvironment.h>
#include <IGUIFont.h>
#include <IGUIStaticText.h>
#include <IGUITabControl.h>
#include <IVideoDriver.h>

#if 0
	#define PRINT_DBG(...) printf(__VA_ARGS__)
#else
	#define PRINT_DBG(...) do {} while(0)
#endif

namespace guilayout {

namespace {
	class Spoofed : public gui::IGUIElement {
	public:
		gui::IGUIEnvironment *getEnv() { return Environment; }
	};
}

IGUIElementWrapper::IGUIElementWrapper(gui::IGUIElement *elem)
{
	setElement(elem);
	if (!m_element) {
		PRINT_DBG("+ IGUIElement (nullptr)\n");
		return;
	}
	PRINT_DBG("+ IGUIElement %s\n", typeid(*m_element).name());
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

	PRINT_DBG("- IGUIElement %s\n", typeid(*m_element).name());

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
			setTextlike(type == gui::EGUIET_BUTTON, 4);
			break;
		case gui::EGUIET_STATIC_TEXT:
			margin = { 0, 1, 1, 1 }; // left align
			expand = { 0, 0 }; // no benefit from larger container
			setTextlike(true, 0);
			{
				gui::IGUIStaticText *e = (gui::IGUIStaticText *)m_element;
				e->setTextAlignment(gui::EGUIA_UPPERLEFT, gui::EGUIA_CENTER);
			}
			break;
		default: break;
	}
}

void IGUIElementWrapper::start(const s16_x4 *pos_new)
{
	if (!m_element)
		return;

	if (pos_new)
		pos = *pos_new;

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

	// Sync with container
	m_element->setRelativePosition(rect);

	// Deal nested elements
	switch (m_element->getType()) {
	case gui::EGUIET_TAB_CONTROL:
		{
			auto tc = (gui::IGUITabControl *)m_element;
			// Update the own element to have correct children information
			// force update tab positions
			tc->setTabVerticalAlignment(tc->getTabVerticalAlignment());

			size_t count = std::min<size_t>(tc->getTabCount(), m_children.size());
			for (size_t i = 0; i < count; ++i) {
				core::recti crect = tc->getTab(i)->getAbsoluteClippingRect();

				m_children[i]->start({
					1, 1,
					(s16)(crect.getWidth() - 2), (s16)(crect.getHeight() - 2)
				});
			}
		}
		break;
	default:
		for (auto &e : m_children) {
			// This container does not do any layouting, thus
			e->start(&pos);
		}
	break;
	}
}

IGUIElementWrapper *IGUIElementWrapper::find_wrapper(Element *e, const gui::IGUIElement *ie)
{
	if (!e)
		return nullptr;

	IGUIElementWrapper *ptr = nullptr;
	e->doRecursive([ie, &ptr] (Element *e_raw) -> bool {
		if (auto e = dynamic_cast<IGUIElementWrapper *>(e_raw)) {
			if (e->m_element == ie)
				ptr = e;
		}
		return !ptr;
	});
	return ptr;
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

bool IGUIElementWrapper::draw_iguiw_wireframe(IGUIElementWrapper *e, video::IVideoDriver *driver, uint32_t color)
{
	if (!e->m_element)
		return true; // maybe a child element is drawable ...?
	if (!e->m_element->isTrulyVisible())
		return false; // hidden.

	video::S3DVertex vertices[8];
	static const u16 indices[10] = { 0, 1, 2, 3, 0, 4, 5, 6, 7, 4 };

	const video::SColor color1 = (color & 0x00FFFFFF) | 0x88000000;
	color = ((color >> 1) ^ 0x13579B) + 0xB97531; // pseudo-random
	const video::SColor color2 = (color & 0x00FFFFFF) | 0x88000000;

	// Show bounding box of the drawn element
	core::recti rect = e->m_element->getAbsoluteClippingRect();
	{
		const auto p00 = rect.UpperLeftCorner;
		const auto p11 = rect.LowerRightCorner;
		vertices[0] = video::S3DVertex(p00.X, p00.Y, 0, 0,0,0, color1, 0,0);
		vertices[1] = video::S3DVertex(p11.X, p00.Y, 0, 0,0,0, color2, 0,0);
		vertices[2] = video::S3DVertex(p11.X, p11.Y, 0, 0,0,0, color1, 0,0);
		vertices[3] = video::S3DVertex(p00.X, p11.Y, 0, 0,0,0, color2, 0,0);
	}

	// Show minimal rect
	core::vector2di center = (rect.UpperLeftCorner + rect.LowerRightCorner) / 2;
	{
		const core::vector2di p00(
			center.X - e->min_size[SIZE_X] / 2,
			center.Y - e->min_size[SIZE_Y] / 2
		);
		const core::vector2di p11(
			center.X + e->min_size[SIZE_X] / 2,
			center.Y + e->min_size[SIZE_Y] / 2
		);
		vertices[4] = video::S3DVertex(p00.X, p00.Y, 0, 0,0,0, color1, 0,0);
		vertices[5] = video::S3DVertex(p11.X, p00.Y, 0, 0,0,0, color2, 0,0);
		vertices[6] = video::S3DVertex(p11.X, p11.Y, 0, 0,0,0, color1, 0,0);
		vertices[7] = video::S3DVertex(p00.X, p11.Y, 0, 0,0,0, color2, 0,0);
	}

	driver->draw2DVertexPrimitiveList(
		vertices, ARRAY_SIZE(vertices),
		indices, ARRAY_SIZE(indices),
		video::EVT_STANDARD, scene::EPT_LINE_LOOP, video::EIT_16BIT
	);
	return true;
}

// FIXME: This function does not take IGUIElement parent offsets into account :(
bool IGUIElementWrapper::draw_table_wireframe(Table *e, video::IVideoDriver *driver, uint32_t color)
{
	if (e->m_children.empty())
		return false; // value does not matter.

	std::vector<video::S3DVertex> vertices;
	std::vector<u16> indices;

	/*
		|  0  1  3  5    7
		| *9            10
		| 11            12
		| 13  2  4  6  8/14
		* = index_y
	*/
	// min pos
	vertices.emplace_back(e->pos[DIR_LEFT], e->pos[DIR_UP], 0, 0,0,0, color, 0,0);
	for (Table::CellInfo &c : e->m_cellinfo[SIZE_X]) {
		vertices.emplace_back(c.pos_minmax[1], e->pos[DIR_UP],   0, 0,0,0, color, 0,0);
		vertices.emplace_back(c.pos_minmax[1], e->pos[DIR_DOWN], 0, 0,0,0, color, 0,0);
	}
	const size_t index_y = vertices.size();
	for (Table::CellInfo &c : e->m_cellinfo[SIZE_Y]) {
		vertices.emplace_back(e->pos[DIR_LEFT],  c.pos_minmax[1], 0, 0,0,0, color, 0,0);
		vertices.emplace_back(e->pos[DIR_RIGHT], c.pos_minmax[1], 0, 0,0,0, color, 0,0);
	}

	const size_t cells_w = e->m_cellinfo[SIZE_X].size();
	const size_t cells_h = e->m_cellinfo[SIZE_Y].size();

	// create the frame
	indices.insert(indices.end(), {
		0, (u16)(cells_w * 2 - 1), // top
		(u16)(cells_w * 2 - 1), (u16)(cells_w * 2), // right
		(u16)(index_y + cells_h * 2 - 2), (u16)(cells_w * 2), // bottom
		0, (u16)(index_y + cells_h * 2 - 2) // left
	});

	// add column separators (X values)
	for (size_t i = 0; i < cells_w; ++i) {
		indices.insert(indices.end(), {
			(u16)(i * 2 + 1), (u16)(i * 2 + 2)
		});
	}

	// add row separators (Y values)
	for (size_t i = 0; i < cells_h; ++i) {
		indices.insert(indices.end(), {
			(u16)(i * 2 + index_y), (u16)(i * 2 + 1 + index_y)
		});
	}

	driver->draw2DVertexPrimitiveList(
		vertices.data(), vertices.size(),
		indices.data(), indices.size() / 2,
		video::EVT_STANDARD, scene::EPT_LINES, video::EIT_16BIT
	);
	return true;
}

void IGUIElementWrapper::draw_wireframe(Element *e, video::IVideoDriver *driver, uint32_t color)
{
	// Needed for the OpenGL 3+ driver
	driver->setMaterial(driver->getMaterial2D());

	auto drawfunc = [driver, &color] (Element *e_raw) -> bool {
		bool ok = true;

		do {
			if (auto e = dynamic_cast<Table *>(e_raw)) {
				ok = draw_table_wireframe(e, driver, color);
				break;
			}

			if (auto e = dynamic_cast<IGUIElementWrapper *>(e_raw)) {
				ok = draw_iguiw_wireframe(e, driver, color);
				break;
			}

			// unhandled: Flexbox
		} while (false);

		color = ((color >> 1) ^ 0x13579B) + 0xB97531; // pseudo-random
		return ok;
	};

	e->doRecursive(drawfunc);
}

void IGUIElementWrapper::setTextlike(bool use_get_text, int margin)
{
	static u16 line_height = 0;
	static u16 avg_letter_width = 0;
	if (line_height == 0) {
		auto font = ((Spoofed *)m_element)->getEnv()->getSkin()->getFont();
		core::dimension2du size = font->getDimension(L"AZyq_m");
		line_height = size.Height + 4;
		avg_letter_width = (size.Width + 6 / 2) / 6;
	}

	float len = 5;
	if (use_get_text)
		len = std::max<u16>(len, wcslen(m_element->getText()));

	min_size = { (s16)(len * avg_letter_width + margin), (s16)(line_height + margin) };
}

}
