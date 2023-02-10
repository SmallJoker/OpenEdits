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
	bid_t id;
	uint8_t param1; // rotation?
};

