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
		ID_SECRET = 50,
		ID_COIN = 100,
		ID_SPAWN = 255,
		ID_INVALID = UINT16_MAX
	};

	// Client-side only flags
	enum Param1Flag : uint8_t {
		P1_FLAG_TILE1 = 0x80
	};

	bool operator ==(const Block &o)
	{
		return id == o.id && bg == o.bg && param1 == o.param1;
	}

	bid_t id = 0; // Foreground block ID
	bid_t bg = 0; // Background block ID

	/*
	Parameter for the foreground blocks
	Portals
		Represents the configuration ID
		Separate list provides:
			rotation in 90° steps
			target ID
			own ID
	Static texts
		Represents the text number (separate list)
	Coins & hidden block (temporary, client-only)
		Collected/activated indicator [0,1]
	*/
	uint8_t param1 = 0;
};

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
