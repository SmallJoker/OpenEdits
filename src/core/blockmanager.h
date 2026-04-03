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

enum class TileOverlayType {
	Text_BottomRight,
	Text_FullSize,
	Invalid
};


/// One of the many possible tiles for a single block
struct BlockTile {
	BlockTile()
	{
		textures.resize(1);
	}

	/// Defines the rendering mode of this tile
	/// Solid+Background: no alpha, Action: alpha_ref, Decoration: alpha
	BlockDrawType type = BlockDrawType::Invalid;
	std::vector<video::ITexture *> textures; // >1 if animated
	bool is_known_tile = false; // true when registered by registerPack()
	bool have_alpha = false; // false: use BlockDrawType
	float animation_delay = 1.0f;
	s16 index = -1; // where to continue looking for frames (< 0 = disabled)

	struct VisualOverride {
		bid_t id;
		uint8_t tile;
		bool enabled = false;
	};
	VisualOverride visual_override; // to use the tile of any block
};

/// Properties of a single block
struct BlockProperties {
	BlockProperties(bid_t id, BlockDrawType type);
	~BlockProperties();

	BlockPack *pack = nullptr;
	const bid_t id;
	BlockParams::Type paramtypes = BlockParams::Type::None;

	// -------------- Visuals -------------

	static const u32 COLOR_DEFAULT_TRANSPARENT = 0x00101010;

	u32 color = COLOR_DEFAULT_TRANSPARENT; // AARRGGBB minimap color
	// maximal count of tiles: 8 (3 bits from Block struct)
	std::vector<BlockTile> tiles; // usually: [0] = normal, [1] = active
	void setTiles(std::vector<BlockDrawType> types);
	const BlockTile &getTileRef(const Block b) const;
	BlockTile getTile(const Block b) const { return getTileRef(b); }

	bool isBackground() const { return tiles[0].type == BlockDrawType::Background; }

	struct Overlay {
		// TODO: Replace type with scale + position

		TileOverlayType type = TileOverlayType::Invalid; // optional
		// Colors: 0xAARRGGBB
		uint32_t fg_color = 0xFFFFFFFF;
		uint32_t bg_color = 0xFF000000;
	};
	Overlay overlay;

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

	// For unittests only!
	#define BP_STEP_CALLBACK(name) \
		void (name)(Player &player, blockpos_t pos)
	BP_STEP_CALLBACK(*step) = nullptr;

	// Lua callbacks. Make sure to update `Script::close` too.
	// Default to -2 == LUA_NOREF
	int ref_intersect_once = -2;
	int ref_on_intersect = -2;
	int ref_on_collide = -2;
	inline bool haveOnIntersectOnce() const { return ref_intersect_once >= 0; }
	inline bool haveOnIntersect()     const { return ref_on_intersect >= 0; }
	inline bool haveOnCollide()       const { return ref_on_collide >= 0; }
#if BUILD_CLIENT
	int ref_get_visuals = -2;
	inline bool haveGetVisuals()       const { return ref_get_visuals >= 0; }

	int ref_gui_def = -2;
	bool have_gui = false;
#endif
};

class BlockManager {
public:
	BlockManager();
	~BlockManager();

	void registerUnittestPacks();

	void registerPack(BlockPack *pack);
	void setDriver(video::IVideoDriver *driver);
	void setMediaMgr(MediaManager *media) { m_media = media; }
	void sanityCheck(); // to run after everything is initialized
	void populateTextures();

	bool isEElike() const { return m_is_ee_like; }

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
	video::ITexture *extractTile(video::ITexture *src, u8 tile_index) const;
	u32 getDominantColor(video::ITexture *texture) const;

	// This is probably a bad idea for headless servers
	video::ITexture *m_missing_texture = nullptr;
	video::IVideoDriver *m_driver = nullptr;
	MediaManager *m_media = nullptr;

	std::vector<BlockProperties *> m_props;
	std::vector<BlockPack *> m_packs;
	bool m_populated = false;
	bool m_is_ee_like = false;
};
