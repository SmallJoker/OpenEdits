#pragma once

#include "types.h"

class World;

struct PositionRange {
	// Not directly used by PositionRange
	enum Operator {
		PROP_SET         = 0x00,
		PROP_ADD         = 0x10,
		PROP_MAX_INVALID = 0x20,
		PROP_MASK        = 0xF0
	} op = PROP_MAX_INVALID;

	enum Type {
		PRT_ONE_BLOCK    = 0x00, // pos
		PRT_AREA         = 0x01, // minpos, maxpos
		PRT_CIRCLE       = 0x02, // pos, radius
		PRT_ENTIRE_WORLD = 0x03, // no args
		PRT_MAX_INVALID  = 0x04,
		PRT_MASK         = 0x0F
	} type = PRT_MAX_INVALID;
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

