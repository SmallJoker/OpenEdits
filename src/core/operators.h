#pragma once

#include "types.h"

class World;

struct PositionRange {
	enum Type : uint8_t {
		T_ONE_BLOCK    = 0x00, // pos
		T_AREA         = 0x01, // minpos, maxpos
		T_CIRCLE       = 0x02, // pos, radius
		T_ENTIRE_WORLD = 0x03, // no args
		T_MAX_INVALID  = 0x04
	} type = T_MAX_INVALID;
	blockpos_t minp, maxp; // max position inclusive
	float radius = 0;

	/// Initializes an iterator
	/// @param ppos: First iterator position.
	/// @return `true` on success
	bool iteratorStart(const World *world, blockpos_t *ppos);

	/// Next iteration.
	/// @return `true` until `ppos` is past the end
	bool iteratorNext(blockpos_t *ppos) const;

private:
	blockpos_t m_center; // for T_CIRCLE
};

