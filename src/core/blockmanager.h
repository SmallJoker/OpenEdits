#pragma once

#include "core/blockparams.h" // enum
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
class BlockManager;

constexpr size_t TEXTURE_SIZE = 32;

// Sorted by tab appearance
enum class BlockDrawType {
	Solid,
	Action,
	Decoration,
	Background,
	Invalid
};

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
	bool is_known_tile = false; // true when registered by registerPack()
	bool have_alpha = false; // false: use BlockDrawType
};

struct BlockProperties {
	BlockProperties(BlockDrawType type);

	BlockPack *pack = nullptr;
	BlockParams::Type paramtypes = BlockParams::Type::None;

	// whether to add the block position to the triggered blocks list
	bool trigger_on_touch = false;

	// -------------- Visuals -------------

	u32 color = 0; // minimap
	// maximal count of tiles: 8 (3 bits from Block struct)
	std::vector<BlockTile> tiles; // usually: [0] = normal, [1] = active
	void setTiles(std::vector<BlockDrawType> types);
	BlockTile getTile(const Block b) const;
	bool isBackground() const { return tiles[0].type == BlockDrawType::Background; }

	// -------------- Physics -------------

	float viscosity = 1;

	enum class CollisionType {
		Position, // and velocity
		Velocity, // just velocity
		None
	};

	// Callback when a player intersects with the block
	#define BP_STEP_CALLBACK(name) \
		void (name)(Player &player, blockpos_t pos)
	BP_STEP_CALLBACK(*step) = nullptr;

	// Callback when colliding: true -> set velocity to 0
	#define BP_COLLIDE_CALLBACK(name) \
		BlockProperties::CollisionType (name)(Player &player, blockpos_t pos, bool is_x)
	BP_COLLIDE_CALLBACK(*onCollide) = nullptr;
};

class BlockManager {
public:
	BlockManager();
	~BlockManager();

	void doPackRegistration();

	void read(Packet &pkt, u16 protocol_version);
	void write(Packet &pkt, u16 protocol_version) const;

	void registerPack(BlockPack *pack);
	void setDriver(video::IVideoDriver *driver);
	void populateTextures();

	const BlockProperties *getProps(bid_t block_id) const;
	const std::vector<BlockProperties *> &getProps() const { return m_props; }
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
	bool m_populated = false;
};
