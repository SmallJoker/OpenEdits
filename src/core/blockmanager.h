#pragma once

#include <string>
#include <map>
#include <vector>

namespace irr {
	namespace video {
		class ITexture;
		class IVideoDriver;
	}
}

typedef uint16_t bid_t;

using namespace irr;

class Player;
class World;

constexpr size_t TEXTURE_SIZE = 32;

enum BlockDrawType {
	Invalid,
	Background,
	Solid,
	Action,
	Decoration
};

struct BlockPack {
	std::string imagepath;
	std::string name;

	BlockDrawType default_type = BlockDrawType::Invalid;

	std::vector<bid_t> block_ids;
};

struct BlockProperties {
	BlockDrawType type = BlockDrawType::Invalid;

	uint32_t color = 0; // minimap
	video::ITexture *texture = nullptr;
	int texture_offset = 0; // e.g. when specifying a material
	int animation_tiles = 0;

	BlockPack *pack = nullptr;

	// Callback when a player intersects with the block
	void (*step)(float dtime, World &world, Player &player) = nullptr;
};

class BlockManager {
public:
	BlockManager();
	~BlockManager();

	void init(video::IVideoDriver *driver) { m_driver = driver; }

	void addNewPack(BlockPack *pack);
	BlockProperties *addToPack(BlockPack *pack, bid_t block_id, const std::string &imagepath);

	BlockProperties *getProps(bid_t block_id);
	BlockPack *getPack(const std::string &name);

private:
	void ensurePropsSize(size_t n);

	// This is probably a bad idea for headless servers
	video::IVideoDriver *m_driver = nullptr;

	std::vector<BlockProperties *> m_props;
	std::vector<BlockPack *> m_packs;
};
