#pragma once

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

struct Block {
	bool operator ==(const Block &o)
	{
		return id == o.id && param1 == o.param1;
	}

	bid_t id = 0;
	uint8_t param1 = 0; // rotation?
};

