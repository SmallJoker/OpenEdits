#pragma once
// Types related to Irrlicht

#include <memory>
#include <vector2d.h>

using namespace irr;

namespace irr {
	namespace core {
		// For std::map insertions
		template <>
		inline u16 roundingError()
		{
			return 0;
		}
	}
}

typedef core::vector2d<u16> blockpos_t;
constexpr u16 BLOCKPOS_INVALID = UINT16_MAX;

struct PositionRange {
	enum Type : uint8_t {
		T_CURRENT_POS  = 0x00, // pos
		T_AREA         = 0x01, // minpos, maxpos
		T_CIRCLE       = 0x02, // pos, radius
		T_ENTIRE_WORLD = 0x03, // no args
		T_MAX_INVALID  = 0x04
	} type = T_MAX_INVALID;
	blockpos_t minp, maxp;
	float radius = 0;
};

typedef uint16_t bid_t;

struct Block {
	explicit Block() : Block(0) {}
	explicit Block(bid_t fg) : id(fg), tile(0) {}

	enum BlockIDs : bid_t {
		ID_KEY_R = 6,
		ID_KEY_G,
		ID_KEY_B,
		ID_DOOR_R = 23,
		ID_DOOR_G,
		ID_DOOR_B,
		ID_GATE_R = 26,
		ID_GATE_G,
		ID_GATE_B,
		ID_COINDOOR = 43,
		ID_COINGATE = 165,
		ID_TIMED_GATE_1 = 156,
		ID_TIMED_GATE_2 = 157,
		ID_SECRET = 50,
		ID_BLACKREAL = 44,
		ID_BLACKFAKE = 243,
		ID_COIN = 100,
		ID_PIANO = 77,
		ID_TELEPORTER = 242,
		ID_SPAWN = 255,
		ID_CHECKPOINT = 360,
		ID_SPIKES = 361,
		ID_TEXT = 1000,
		ID_INVALID = UINT16_MAX
	};
	static constexpr uint8_t TILES_MAX = (1 << 4) - 1;

	// Apparently we cannot use "union" to write to both fields at once because MSVC is a bitch
	bid_t id : 12;    // Foreground block ID (max. 4000)
	uint8_t tile : 4; // Tile number (client-side only, for rendering)

	bid_t bg = 0; // Background block ID
};

// 4 bytes with GCC, 6 bytes with MSVC
static_assert(sizeof(Block) <= 6, "Block size us unexpectedly large");


// Automatic grab & drop for irr::IReferenceCounted classes
// Similar to std::shared_ptr but less thread-safe
template<typename T>
using RefCnt = std::shared_ptr<T>;

