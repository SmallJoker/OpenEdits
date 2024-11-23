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

	void updatePosition() override;
	void doRecursive(std::function<bool(Element *)> callback) override;

	static void draw_wireframe(Element *e, video::IVideoDriver *driver, uint32_t color);

	template <typename T, typename ... Args>
	T *add(Args &&... args)
	{
		m_children.push_back(std::make_unique<T>(std::forward<Args>(args)...));
		return dynamic_cast<T *>(m_children.back().get());
	}

protected:
	gui::IGUIElement *m_element = nullptr;
	std::vector<std::unique_ptr<Element>> m_children; // e.g. tabs

private:
	void setElement(gui::IGUIElement *elem);
	void setTextlike(bool use_get_text);

	static bool draw_iguiw_wireframe(IGUIElementWrapper *e, video::IVideoDriver *driver, uint32_t color);
	static bool draw_table_wireframe(Table *e, video::IVideoDriver *driver, uint32_t color);
};

}
