#include "minimap.h"
#include "gameplay.h"
#include "client/client.h"
#include "core/blockmanager.h"
#include <IGUIEnvironment.h>
#include <IGUIImage.h>
#include <IImage.h>
#include <IVideoDriver.h>

static constexpr int ID_ImgMinimap = 30;

SceneMinimap::SceneMinimap(SceneGameplay *parent, Gui *gui)
{
	m_gameplay = parent;
	m_gui = gui;
}

SceneMinimap::~SceneMinimap()
{
	m_gui->driver->removeTexture(m_texture);
}

void SceneMinimap::draw()
{
	if (!m_is_visible)
		return;

	// Add the minimap texture
	auto gui = m_gui->guienv;

	core::recti rect = m_gameplay->getDrawArea();
	rect.UpperLeftCorner = rect.LowerRightCorner - core::dimension2di(m_imgsize);

	auto e = gui->addImage(rect, nullptr, ID_ImgMinimap, nullptr, false);
	e->setImage(m_texture);
}

void SceneMinimap::step(float dtime)
{
	if (!m_is_dirty || !m_is_visible)
		return;
	m_is_dirty = false;

	auto world = m_gui->getClient()->getWorld();

	const size_t width = world->getSize().X + 4; // 2 px border
	const size_t height = world->getSize().Y + 4;

	core::dimension2du size_new(width, height);
	auto img = m_gui->driver->createImage(video::ECF_R8G8B8, size_new);

	for (size_t y = 0; y < height; ++y)
	for (size_t x = 0; x < width; ++x) {
		Block b;
		if (!world->getBlock(blockpos_t(x - 2, y - 2), &b)) {
			img->setPixel(x, y, 0xFF444444);
			continue;
		}

		bool have_solid_above = false;
		do {
			if (b.id == 0)
				break;

			auto props = g_blockmanager->getProps(b.id);
			video::SColor color(0xFFFF0000); // default red
			if (props) {
				if (props->type != BlockDrawType::Solid)
					break;
				color = props->color;
			}

			img->setPixel(x, y, color);
			have_solid_above = true;
		} while (0);


		do {
			if (have_solid_above)
				break;

			auto props = g_blockmanager->getProps(b.bg);
			video::SColor color(0xFFFF0000); // default red
			if (props)
				color = props->color;

			img->setPixel(x, y, color);
		} while (0);
	}

	m_gui->driver->removeTexture(m_texture);
	m_texture = m_gui->driver->addTexture("&&minimap", img);

	img->drop();

	if (m_imgsize != size_new)
		m_imgsize = size_new;

	{
		// Force update element
		toggleVisibility(); // remove
		m_is_visible = true;

		draw();
	}
}

void SceneMinimap::toggleVisibility()
{
	m_is_visible ^= true;

	if (m_is_visible) {
		draw();
	} else {
		auto root = m_gui->guienv->getRootGUIElement();
		auto e = root->getElementFromId(ID_ImgMinimap);
		if (e)
			root->removeChild(e);
	}
}
