#include "minimap.h"
#include "gameplay.h"
#include "client/client.h"
#include "core/blockmanager.h"
#include "core/player.h"
#include <IGUIEnvironment.h>
#include <IGUIImage.h>
#include <IImage.h>
#include <IVideoDriver.h>

#if 0
	#define DEBUGLOG(...) printf(__VA_ARGS__)
#else
	#define DEBUGLOG(...) /* SILENCE */
#endif
#define ERRORLOG(...) fprintf(stderr, __VA_ARGS__)

constexpr int ID_ImgBlocks = 30,
	ID_ImgOverlay = 31;
constexpr int BORDER = 2; // px

SceneMinimap::SceneMinimap(SceneGameplay *parent, Gui *gui)
{
	m_gameplay = parent;
	m_gui = gui;
}

SceneMinimap::~SceneMinimap()
{
	m_gui->driver->removeTexture(m_blocks_txt);
	m_gui->driver->removeTexture(m_overlay_txt);

	if (m_overlay_img)
		m_overlay_img->drop();
}

void SceneMinimap::draw()
{
	m_blocks_elm = nullptr;
	m_overlay_elm = nullptr;

	if (!m_is_visible)
		return;

	DEBUGLOG("Minimap: draw\n");

	// Add the minimap texture
	auto gui = m_gui->guienv;
	auto root = gui->getRootGUIElement();

	core::recti rect = m_gameplay->getDrawArea();
	rect.UpperLeftCorner = rect.LowerRightCorner - core::dimension2di(m_imgsize);


	// Actual minimap texture
	auto e = (gui::IGUIImage *)root->getElementFromId(ID_ImgBlocks);
	if (e) {
		// Force update: size changed
		e->setRelativePosition(rect);
	} else {
		e = gui->addImage(rect, nullptr, ID_ImgBlocks, nullptr, false);
	}
	e->setImage(m_blocks_txt);
	m_blocks_elm = e;


	// Players and other effects
	e = (gui::IGUIImage *)root->getElementFromId(ID_ImgOverlay);
	if (e) {
		// Force update: size changed
		e->setRelativePosition(rect);
	} else {
		e = gui->addImage(rect, nullptr, ID_ImgOverlay, nullptr, false);
		e->setUseAlphaChannel(true);
	}
	e->setImage(m_overlay_txt);
	m_overlay_elm = e;
}

void SceneMinimap::step(float dtime)
{
	updateMap();
	updatePlayers(dtime);

	if (m_need_force_reload) {
		m_need_force_reload = false;
		DEBUGLOG("Minimap: force reload\n");

		// Force update element
		draw();
	}
}


void SceneMinimap::updateMap()
{
	if (!m_is_dirty || !m_is_visible)
		return;
	m_is_dirty = false;

	auto world = m_gui->getClient()->getWorld();

	const size_t width = world->getSize().X + 2 * BORDER;
	const size_t height = world->getSize().Y + 2 * BORDER;

	core::dimension2du size_new(width, height);
	auto img = m_gui->driver->createImage(video::ECF_R8G8B8, size_new);

	for (size_t y = 0; y < height; ++y)
	for (size_t x = 0; x < width; ++x) {
		Block b;
		if (!world->getBlock(blockpos_t(x - BORDER, y - BORDER), &b)) {
			// 2px border
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
				auto tile = props->getTile(b);
				if (tile.type != BlockDrawType::Solid)
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

	m_gui->driver->removeTexture(m_blocks_txt);
	m_blocks_txt = m_gui->driver->addTexture("&&minimap", img);

	img->drop();

	if (m_imgsize == size_new) {
		m_blocks_elm->setImage(m_blocks_txt);
	} else {
		requestSizeUpdate();
	}

	m_imgsize = size_new;
}

void SceneMinimap::updatePlayers(float dtime)
{
	if (!m_is_visible) {
		if (m_overlay_img) {
			// Clear movement traces as they become outdated quickly.
			m_overlay_img->drop();
			m_overlay_img = nullptr;
		}
		return;
	}

	{
		// Limit update rate
		m_overlay_timer += dtime;
		if (m_overlay_timer < 0.03f)
			return;
		m_overlay_timer -= 0.03f;
	}

	// TODO: Find a way to optimize this
	auto img = m_overlay_img;
	if (!img) {
		img = m_gui->driver->createImage(video::ECF_A8R8G8B8, m_imgsize);
		img->fill(0x00000000);
		m_overlay_img = img;
	}

	{
		// Fade out the trail
		if (img->getBytesPerPixel() != 4)
			throw std::runtime_error("Unexpected pixel format");

		const size_t length = img->getImageDataSizeInBytes();
		u8 *data = (u8 *)img->getData();
		for (size_t i = 3; i < length; i += 4) {
			if (data[i])
				data[i] -= 15; // 255 / 15
		}
	}

	auto myself = m_gui->getClient()->getMyPeerId();
	auto players = m_gui->getClient()->getPlayerList();
	for (auto player : *players.ptr()) {
		auto pos = player.second->pos;

		video::SColor color(0xFFFFFFFF); // default white
		if (player.second->peer_id == myself)
			color = 0xFF44FF44; // green highlight

		int cx = std::round(pos.X) + BORDER;
		int cy = std::round(pos.Y) + BORDER;

		for (int y = cy - 1; y <= cy + 1; ++y)
		for (int x = cx - 1; x <= cx + 1; ++x) {
			// Under- and overflow protected
			img->setPixel(x, y, color);
		}
	}

	m_gui->driver->removeTexture(m_overlay_txt);
	m_overlay_txt = m_gui->driver->addTexture("&&mmplayers", img);
	m_overlay_elm->setImage(m_overlay_txt);
}


void SceneMinimap::toggleVisibility()
{
	m_is_visible ^= true;
	DEBUGLOG("Minimap: set visibility to: %d\n", (int)m_is_visible);

	if (m_blocks_elm)
		m_blocks_elm->setVisible(m_is_visible);

	if (m_overlay_elm)
		m_overlay_elm->setVisible(m_is_visible);

	if (m_is_visible)
		draw();
}


void SceneMinimap::requestSizeUpdate()
{
	m_need_force_reload = true;

	if (m_overlay_img) {
		m_overlay_img->drop();
		m_overlay_img = nullptr;
	}
}
