#pragma once
#include "guilayout.h"

namespace irr {
	namespace gui {
		class IGUIElement;
	}
	namespace video {
		class IVideoDriver;
	}
}

namespace guilayout {

using namespace irr;
struct IGUIElementWrapper : public Element {
	IGUIElementWrapper(gui::IGUIElement *elem = nullptr);
	virtual ~IGUIElementWrapper();

	gui::IGUIElement *getElement() const { return m_element; }

	void start(const u16_x4 *pos_new) override;

	static IGUIElementWrapper *find_wrapper(Element *e, const gui::IGUIElement *ie);
	static void draw_wireframe(Element *e, video::IVideoDriver *driver, uint32_t color);

	// Same as `FlexBox::add`
	template <typename T, typename ... Args>
	T *add(Args &&... args)
	{
		m_children.push_back(std::make_unique<T>(std::forward<Args>(args)...));
		return dynamic_cast<T *>(m_children.back().get());
	}

	template <typename T>
	T *add(T *ptr)
	{
		return dynamic_cast<T *>(m_children.emplace_back(ptr).get());
	}

	Element *get(size_t i) { return m_children.at(i).get(); }

protected:
	gui::IGUIElement *m_element = nullptr;

private:
	void setElement(gui::IGUIElement *elem);
	void setTextlike(bool use_get_text);

	static bool draw_iguiw_wireframe(IGUIElementWrapper *e, video::IVideoDriver *driver, uint32_t color);
	static bool draw_table_wireframe(Table *e, video::IVideoDriver *driver, uint32_t color);
};

}
