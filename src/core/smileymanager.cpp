#include "smileymanager.h"
#include "logger.h"
#include "macros.h"
#include "mediamanager.h"
#include <IVideoDriver.h>
#include <memory> // unique_ptr

static Logger logger("SmileyManager", LL_INFO);

SmileyManager::~SmileyManager()
{
	for (SmileyPack *pack : m_smiley_packs) {
		if (m_driver && pack->texture)
			m_driver->removeTexture(pack->texture);
		delete pack;
	}
}

void SmileyManager::init(video::IVideoDriver *driver, MediaManager *media)
{
	m_driver = driver;
	m_media = media;
}

void SmileyManager::registerHardcoded()
{
	auto pack = std::make_unique<SmileyPack>("basic");
	pack->defs.resize(5);
	registerPack(pack.release());
}

void SmileyManager::registerPack(SmileyPack *toadd)
{
	ASSERT_FORCED(m_media, "Missing media");
	ASSERT_FORCED(!m_populated, "too late");

	bool ok = m_media->requireAsset(("smileys_" + toadd->name + ".png").c_str());
	ASSERT_FORCED(ok, "Smiley texture not found");

	// Attempt to replace
	bool replaced = false;
	for (SmileyPack *&pack : m_smiley_packs) {
		if (pack->name == toadd->name) {
			delete pack;
			pack = toadd;
			replaced = true;
			break;
		}
	}

	if (!replaced) {
		m_smiley_packs.emplace_back(toadd);
	}
}

void SmileyManager::populateTextures()
{
	ASSERT_FORCED(!m_populated && m_media, "invalid call");
	if (!m_driver)
		return; // headless client

	if (m_smiley_packs.empty()) {
		// Fallback
		registerHardcoded();
	}

	m_populated = true;

	video::ITexture *tex_fallback = m_driver->getTexture("assets/textures/missing_texture.png");

	size_t count = 0;
	for (SmileyPack *pack : m_smiley_packs) {
		const std::string imagepath = "smileys_" + pack->name + ".png";
		const char *path = m_media->getAssetPath(imagepath.c_str());

		video::ITexture *texture = nullptr;
		if (path)
			texture = m_driver->getTexture(path);
		if (texture) {
			auto img_dim = texture->getOriginalSize();
			pack->texture_width = img_dim.Width / img_dim.Height;
		} else {
			logger(LL_ERROR, "Failed to load texture '%s'", path);
			texture = tex_fallback;
		}


		pack->texture = texture;
		count += pack->defs.size();
	}

	logger(LL_PRINT, "Registered %zu smileys in %zu textures", count, m_smiley_packs.size());
}

size_t SmileyManager::getCount() const
{
	size_t count = 0;
	for (const SmileyPack *pack : m_smiley_packs) {
		count += pack->defs.size();
	}
	return count;
}

std::pair<const SmileyPack *, size_t> SmileyManager::getSmileyAt(int i_int) const
{
	ASSERT_FORCED(m_populated, "not ready!");

	size_t i = (i_int > 0) * i_int;

	for (const SmileyPack *pack : m_smiley_packs) {
		if (i < pack->defs.size())
			return { pack, i };

		i -= pack->defs.size();
	}

	static const SmileyPack fallback("ERR");
	return { &fallback, 0 };
}

