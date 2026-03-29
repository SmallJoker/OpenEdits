#pragma once

#include <string>
#include <utility> // pair
#include <vector>

namespace irr::video {
	class ITexture;
	class IVideoDriver;
}

using namespace irr;

class MediaManager;

struct SmileyDef {
	std::string description;
};

struct SmileyPack {
	SmileyPack(const std::string &name) :
		name(name) {}

	std::string name;
	video::ITexture *texture = nullptr;
	int texture_width = 1; //< Max count of smileys for this texture
	std::vector<SmileyDef> defs; //< Actual amount of smileys
};

class SmileyManager {
public:
	~SmileyManager();
	void init(video::IVideoDriver *driver, MediaManager *media);

	void registerHardcoded();

	void registerPack(SmileyPack *pack);
	void populateTextures();

	size_t getCount() const;
	std::pair<const SmileyPack *, size_t> getSmileyAt(int i) const;

	const std::vector<SmileyPack *> &getSmileyPacks() const
	{ return m_smiley_packs; }

private:
	video::IVideoDriver *m_driver = nullptr;
	MediaManager *m_media = nullptr;

	std::vector<SmileyPack *> m_smiley_packs;
	bool m_populated = false;
};
