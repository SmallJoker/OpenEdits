#pragma once
// Types related to Irrlicht

#include <IReferenceCounted.h>
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
typedef uint16_t bid_t;
constexpr u16 BLOCKPOS_INVALID = UINT16_MAX;

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
		ID_SECRET = 50,
		ID_COIN = 100,
		ID_SPAWN = 255,
		ID_CHECKPOINT = 360,
		ID_SPIKES = 361,
		ID_INVALID = UINT16_MAX
	};

	// Apparently we cannot use "union" to write to both fields at once because MSVC is a bitch
	bid_t id : 13;    // Foreground block ID (max. 8000)
	uint8_t tile : 3; // Tile number (client-side only, for rendering)

	bid_t bg = 0; // Background block ID
};

// 4 bytes with GCC, 6 bytes with MSVC
static_assert(sizeof(Block) <= 6, "Block size us unexpectedly large");


// Automatic grab & drop for irr::IReferenceCounted classes
// Similar to std::shared_ptr but less thread-safe
template<typename T>
class RefCnt {
public:
	RefCnt(irr::IReferenceCounted *ptr)
	{
		if (ptr)
			ptr->grab();
		m_ptr = ptr;
	}
	~RefCnt()
	{
		if (m_ptr)
			m_ptr->drop();
	}

	// Copy constructor
	RefCnt(const RefCnt &o) : RefCnt(o.m_ptr) {}

	RefCnt &operator=(const RefCnt &o)
	{
		if (o.m_ptr)
			o.m_ptr->grab();
		if (m_ptr)
			m_ptr->drop();
		m_ptr = o.m_ptr;
		return *this;
	}

	inline T *ptr() const { return (T *)m_ptr; }
	// Synthetic sugar
	inline T *operator->() const { return (T *)m_ptr; }
	inline bool operator!=(const RefCnt &o) const { return m_ptr != o.m_ptr; }
	// Validation check
	inline bool operator!() const { return !m_ptr; }

private:
	irr::IReferenceCounted *m_ptr;
};
