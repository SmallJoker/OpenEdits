#pragma once

#include "core/types.h"
#include <string>
#include <map>
#include <vector>
#include <vector2d.h> // collision direction

namespace irr {
	namespace video {
		class ITexture;
		class IVideoDriver;
	}
}

class Player;
class World;
class BlockManager;
struct CollisionData;

constexpr size_t TEXTURE_SIZE = 32;

// Sorted by tab appearance
enum class BlockDrawType {
	Solid,
	Action,
	Decoration,
	Background,
	Invalid
};

// Depends on param1
enum class BlockTileCondition {
	NotZero,
	Zero,
	None
};

extern BlockManager *g_blockmanager;

struct BlockPack {
	BlockPack(const std::string &name) :
		name(name) {}
	std::string imagepath;
	std::string name;

	BlockDrawType default_type = BlockDrawType::Invalid;

	std::vector<bid_t> block_ids;
};

struct BlockTile {
	BlockDrawType type = BlockDrawType::Invalid;
	video::ITexture *texture = nullptr;
	u8 texture_offset = 0; // e.g. when specifying a material
};

struct BlockProperties {
	BlockProperties(BlockDrawType type);

	BlockPack *pack = nullptr;

	u32 color = 0; // minimap

	BlockTile tiles[2]; // normal, active
	BlockTile getTile(const Block b) const;
	BlockTileCondition condition = BlockTileCondition::None;

	// Callback when a player intersects with the block
	#define BP_STEP_CALLBACK(name) \
		void (name)(Player &player, blockpos_t pos)
	BP_STEP_CALLBACK(*step) = nullptr;

	// Callback when colliding: true -> set velocity to 0
	#define BP_COLLIDE_CALLBACK(name) \
		bool (name)(Player &player, const core::vector2d<s8> dir)
	BP_COLLIDE_CALLBACK(*onCollide) = nullptr;
};

class BlockManager {
public:
	BlockManager();
	~BlockManager();

	void registerPack(BlockPack *pack);
	void populateTextures(video::IVideoDriver *driver);

	BlockProperties *getProps(bid_t block_id);
	BlockPack *getPack(const std::string &name);
	const std::vector<BlockPack *> &getPacks() { return m_packs; }
	video::ITexture *getMissingTexture() { return m_missing_texture; }

private:
	void ensurePropsSize(size_t n);
	u32 getBlockColor(const BlockTile tile) const;

	// This is probably a bad idea for headless servers
	video::ITexture *m_missing_texture = nullptr;
	video::IVideoDriver *m_driver = nullptr;

	BlockProperties m_fallback;
	std::vector<BlockProperties *> m_props;
	std::vector<BlockPack *> m_packs;
};
