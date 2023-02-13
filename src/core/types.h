#pragma once
// Types related to Irrlicht

#include <IReferenceCounted.h>
#include <vector3d.h>

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

typedef core::vector3d<u16> blockpos_t;
typedef uint16_t bid_t;
constexpr bid_t BLOCKID_INVALID = UINT16_MAX;

struct Block {
	bool operator ==(const Block &o)
	{
		return id == o.id && param1 == o.param1;
	}

	bid_t id = 0;
	/*
	Portals
		Represents the configuration ID
		Separate list provides:
			rotation in 90° steps
			target ID
			own ID
	Static texts
		Represents the text number (separate list)
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