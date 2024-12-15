#include "operators.h"
#include "world.h"

bool PositionRange::iteratorStart(const World *world, blockpos_t *ppos)
{
	if (!world)
		return false;

	const blockpos_t wmaxp = world->getSize() - blockpos_t(1, 1);

	switch (type) {
	case T_ONE_BLOCK:
		*ppos = minp;
		return world->isValidPosition(minp.X, minp.Y);
	case T_CIRCLE:
		{
			if (radius <= 0)
				return false; // 0 blocks affected

			m_center = minp;
			minp.X = std::max<f32>(0, std::round(m_center.X - radius));
			minp.Y = std::max<f32>(0, std::round(m_center.Y - radius));
			maxp.X = std::max<f32>(0, std::round(m_center.X + radius));
			maxp.Y = std::max<f32>(0, std::round(m_center.Y + radius));
		}
		// fall-through
	case T_AREA:
		// scissor minp/maxp area
		{
			minp.X = std::min(minp.X, wmaxp.X);
			minp.Y = std::min(minp.Y, wmaxp.Y);
			maxp.X = std::min(maxp.X, wmaxp.X);
			maxp.Y = std::min(maxp.Y, wmaxp.Y);
		}
		*ppos = minp;
		--(ppos->X); // one step back
		return iteratorNext(ppos);
	case T_ENTIRE_WORLD:
		minp = blockpos_t(0, 0);
		maxp = wmaxp;

		*ppos = minp;
		--(ppos->X); // one step back
		return iteratorNext(ppos);
	case T_MAX_INVALID:
		return false;
	}
	throw std::runtime_error("invalid PRT");
}

bool PositionRange::iteratorNext(blockpos_t *ppos) const
{
	blockpos_t pos = *ppos;

	switch (type) {
	case T_ONE_BLOCK:
		return false;
	case T_AREA:
	case T_CIRCLE:
	case T_ENTIRE_WORLD:
		++pos.X;

		for (; pos.Y <= maxp.Y; ++pos.Y) {
			for (; pos.X <= maxp.X; ++pos.X) {
				if (radius) {
					if ((m_center - pos).getLengthSQ() > radius * radius)
						continue;
				}

				goto done;
			}
			pos.X = minp.X;
		}
		return false;
	case T_MAX_INVALID:
		return false;
	}

done:
	*ppos = pos;
	return true;
}
