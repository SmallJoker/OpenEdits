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
class MediaManager;

constexpr size_t TEXTURE_SIZE = 32;

// Sorted by tab appearance
enum class BlockDrawType {
	Solid,
	Action,
	Decoration,
	Background,
	Invalid
};

/// A pack may contain multiple block IDs, which have their own BlockProperties instance.
struct BlockPack {
	BlockPack(const std::string &name) :
		name(name) {}
	std::string imagepath;
	std::string name;

	/// Describes the tab where the pack shows up
	BlockDrawType default_type = BlockDrawType::Invalid;

	std::vector<bid_t> block_ids;
};

/// One of the many possible tiles for a single block
struct BlockTile {
	/// Defines the rendering mode of this tile
	/// Solid+Background: no alpha, Action: alpha_ref, Decoration: alpha
	BlockDrawType type = BlockDrawType::Invalid;
	video::ITexture *texture = nullptr;
	bool is_known_tile = false; // true when registered by registerPack()
	bool have_alpha = false; // false: use BlockDrawType
};

/// Properties of a single block
struct BlockProperties {
	BlockProperties(bid_t id, BlockDrawType type);
	~BlockProperties();

	BlockPack *pack = nullptr;
	const bid_t id;
	BlockParams::Type paramtypes = BlockParams::Type::None;

	// whether to add the block position to the touched blocks list
	bool trigger_on_touch = false;

	// -------------- Visuals -------------

	u32 color = 0; // AARRGGBB minimap color
	// maximal count of tiles: 8 (3 bits from Block struct)
	std::vector<BlockTile> tiles; // usually: [0] = normal, [1] = active
	void setTiles(std::vector<BlockDrawType> types);
	BlockTile getTile(const Block b) const;
	bool isBackground() const { return tiles[0].type == BlockDrawType::Background; }

	// -------------- Physics -------------

	/// Whether the physics depend on the tile index
	/// This means, any tile change must be broadcast to all players in the world
	/// to ensure proper physics predictions.
	/// -1: not specified. 0: not dependent, 1: is dependent
	int8_t tile_dependent_physics = -1;

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

	// Lua callbacks. Make sure to update `Script::close` too.
	int ref_on_placed = -2; // LUA_NOREF
	int ref_intersect_once = -2; // LUA_NOREF
	int ref_on_intersect = -2; // LUA_NOREF
	int ref_on_collide = -2; // LUA_NOREF
	inline bool haveOnPlaced()        const { return ref_on_placed >= 0; }
	inline bool haveOnIntersectOnce() const { return ref_intersect_once >= 0; }
	inline bool haveOnIntersect()     const { return ref_on_intersect >= 0; }
	inline bool haveOnCollide()       const { return ref_on_collide >= 0; }
#if BUILD_CLIENT
	int ref_gui_def = -2; // LUA_NOREF
#endif
};

class BlockManager {
public:
	BlockManager();
	~BlockManager();

	void doPackRegistration();
	void doPackPostprocess();

	void registerPack(BlockPack *pack);
	void setDriver(video::IVideoDriver *driver);
	void setMediaMgr(MediaManager *media) { m_media = media; }
	void sanityCheck(); // to run after everything is initialized
	void populateTextures();

	bool isEElike() const { return m_is_ee_like; }
	bool isHardcoded() const { return m_hardcoded_packs; }

	// Blocks
	const BlockProperties *getProps(bid_t block_id) const;;
	const std::vector<BlockProperties *> &getProps() const { return m_props; }

	// Only for Script
	BlockProperties *getPropsForModification(bid_t block_id) const;
	std::vector<BlockProperties *> &getPropsForModification() { return m_props; }

	// Packs
	const BlockPack *getPack(const std::string &name) const;
	const std::vector<BlockPack *> &getPacks() { return m_packs; }

	// Client
	video::ITexture *getMissingTexture() { return m_missing_texture; }

private:
	void ensurePropsSize(size_t n);
	u32 getBlockColor(const BlockTile tile) const;

	// This is probably a bad idea for headless servers
	video::ITexture *m_missing_texture = nullptr;
	video::IVideoDriver *m_driver = nullptr;
	MediaManager *m_media = nullptr;

	std::vector<BlockProperties *> m_props;
	std::vector<BlockPack *> m_packs;
	bool m_hardcoded_packs = false;
	bool m_populated = false;
	bool m_is_ee_like = false;
};
