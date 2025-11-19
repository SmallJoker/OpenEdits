#include "guilayout.h"

// This adds 1 MiB to the ELF with -gdwarf
// That's way more than expected. Where does that bloat come from? STL headers?

namespace guilayout {

/*
	Not implemented yet:
	- Scrollbar offsets + Tests
	- Scrollbar drawing
	- Fixed aspect ratio
	- Optimizations?
*/

constexpr s16 SCROLLBAR_SIZE = 24;

#if 0
	#define DBG_TABLE(...) printf(__VA_ARGS__)
	#define DBG_TABLE_LUT
#else
	#define DBG_TABLE(...) do {} while(0)
#endif

void Element::doRecursive(std::function<bool(Element *)> callback)
{
	if (!callback(this))
		return;

	for (auto &e : m_children) {
		if (e.get())
			e->doRecursive(callback);
	}
}

void Element::start(const s16_x4 *pos_new)
{
	if (pos_new) {
		pos = *pos_new;

		// TODO: If custom positions are provided,
		// `getMinSize()` may be called multiple times.

		// 1. Get the desired size of all children
		getMinSize(false, false);

		// 2. Get the minimal size to avoid scrollbars
		// Disabled due to unknown size contraints of the parent

	} // else: size calculations already done by parent

	// 3. Determine whether we need space for scrollbars

	// 4. Update the positions of the child elements
	tryFitElements();

	// 5. Position update of all children (recursive)
	for (auto &e : m_children) {
		if (e)
			e->start(nullptr);
	}
}


void Table::getMinSize(bool shrink_x, bool shrink_y)
{
	// Find the minimal dimensions for each column and row by checking every cell
	for (size_t y = 0; y < m_cellinfo[SIZE_Y].size(); ++y) {
		CellInfo &row = m_cellinfo[SIZE_Y][y];
		row.min_size = 0; // reset from previous calls

		for (size_t x = 0; x < m_cellinfo[SIZE_X].size(); ++x) {
			CellInfo &col = m_cellinfo[SIZE_X][x];
			if (y == 0)
				col.min_size = 0; // reset from previous calls

			Element *e = m_children[y * m_cellinfo[SIZE_X].size() + x].get();
			if (!e)
				continue;

			e->getMinSize(shrink_x, shrink_y);

			col.min_size = std::max<s16>(col.min_size, e->min_size[SIZE_X]);
			row.min_size = std::max<s16>(row.min_size, e->min_size[SIZE_Y]);
		}
	}

	// Total "min" size
	s16 fixed_col = 0;
	s16 fixed_row = 0;

	for (CellInfo &ci : m_cellinfo[SIZE_X])
		fixed_col += ci.min_size;
	for (CellInfo &ci : m_cellinfo[SIZE_Y])
		fixed_row += ci.min_size;

	min_size = { fixed_col, fixed_row };
}

void Table::tryFitElements()
{
	s16_x2 total_space {
		(s16)(pos[DIR_RIGHT] - pos[DIR_LEFT]),
		(s16)(pos[DIR_DOWN]  - pos[DIR_UP])
	};

	m_scrollbars[SIZE_X] = min_size[SIZE_X] > total_space[SIZE_X];
	m_scrollbars[SIZE_Y] = min_size[SIZE_Y] > total_space[SIZE_Y];

	if (m_scrollbars[SIZE_X])
		total_space[SIZE_Y] -= SCROLLBAR_SIZE; // X-axis needs Y space
	if (m_scrollbars[SIZE_Y])
		total_space[SIZE_X] -= SCROLLBAR_SIZE; // Y-axis needs X space

	for (Size dim : { SIZE_X, SIZE_Y }) {
		// Total space and weights are now known: spread.
		spreadTable(dim, total_space[dim]);
	}

	// Now center the cell contents
	for (size_t y = 0; y < m_cellinfo[SIZE_Y].size(); ++y)
	for (size_t x = 0; x < m_cellinfo[SIZE_X].size(); ++x) {
		Element *e = m_children[y * m_cellinfo[SIZE_X].size() + x].get();
		if (!e)
			continue;

		spreadCell(e, x, SIZE_X);
		spreadCell(e, y, SIZE_Y);
	}
}

void Table::spreadTable(Size dim, s16 total_space)
{
	const Direction dir_L = (dim == SIZE_X) ? DIR_LEFT : DIR_UP;

	s16 total_weights = 0;
	s16 dynamic_space = total_space;

	{
		s16 weights_all = 0;
		for (CellInfo &ci : m_cellinfo[dim])
			weights_all += ci.weight;
		weights_all = std::max<s16>(1, weights_all);

		for (CellInfo &ci : m_cellinfo[dim]) {
			total_weights += ci.weight;
			dynamic_space -= ci.min_size;
		}

		// Use new weights that take content fitting into account
		total_weights = std::max<s16>(1, total_weights);
		// < 0 means the available space is insufficient
		dynamic_space = std::max<s16>(0, dynamic_space);
	}

	s16 fixed_offset = pos[dir_L]/*- m_scroll_offset[dim]*/;
	s16 weight_sum = 0;

	for (CellInfo &ci : m_cellinfo[dim]) {
		ci.pos_minmax[0] =
			+ fixed_offset
			+ dynamic_space * weight_sum / total_weights;

		weight_sum += ci.weight;
		fixed_offset += ci.min_size;

		ci.pos_minmax[1] =
			+ fixed_offset
			+ dynamic_space * weight_sum / total_weights;
	}
}

#ifdef DBG_TABLE_LUT
static const char *spread_label[2] = { "-> X", "   Y" };
#endif

void Table::spreadCell(Element *e, size_t num, Size dim)
{
	Direction dir_L = (dim == SIZE_X) ? DIR_LEFT : DIR_UP;
	Direction dir_R = (dim == SIZE_X) ? DIR_RIGHT : DIR_DOWN;

	s16 weight_sum = e->margin[dir_L];
	s16 total_weights =
		+ weight_sum
		+ e->expand[dim] * 2
		+ e->margin[dir_R];
	if (total_weights == 0)
		total_weights = 1;

	CellInfo &ci = m_cellinfo[dim][num];
	s16 fixed_offset = ci.pos_minmax[0];
	s16 fixed_space = e->min_size[dim];
	s16 dynamic_space = ci.pos_minmax[1] - ci.pos_minmax[0] - fixed_space;

	DBG_TABLE("%s%zu, Min % 4i, Max % 4i, Fix % 3i, Dyn % 4i\n",
			spread_label[dim], num, ci.pos_minmax[0], ci.pos_minmax[1], fixed_space, dynamic_space);

	e->pos[dir_L] =
		+ fixed_offset // static offset
		+ dynamic_space * weight_sum / total_weights;

	weight_sum += e->expand[dim] * 2;
	fixed_offset += fixed_space;

	e->pos[dir_R] =
		+ fixed_offset // static offset
		+ dynamic_space * weight_sum / total_weights;
}


#if 0
	#define DBG_FLEXBOX(...) printf(__VA_ARGS__)
#else
	#define DBG_FLEXBOX(...) do {} while(0)
#endif

void FlexBox::tryFitElements()
{
	// It is easier to think in 1 direction (X, left to right)
	const Direction
		dir_L = (box_axis == SIZE_X) ? DIR_LEFT : DIR_UP,
		dir_U = (box_axis == SIZE_X) ? DIR_UP : DIR_LEFT,
		dir_D = (box_axis == SIZE_X) ? DIR_DOWN : DIR_RIGHT;
	const Size box_axis_O = (box_axis == SIZE_X) ? SIZE_Y : SIZE_X;

	s16_x2 total_space {
		(s16)(pos[DIR_RIGHT] - pos[DIR_LEFT]),
		(s16)(pos[DIR_DOWN]  - pos[DIR_UP])
	};

	// FIXME: The wrap outcome depends on whether we need space for a scrollbar
	// but we can only determine whether a scrollbar is needed after wrapping.
	// Compromise: require enough space beforehand. See `FlexBox::getMinSize`.
	m_scrollbars[SIZE_X] = min_size[SIZE_X] > total_space[SIZE_X];
	m_scrollbars[SIZE_Y] = min_size[SIZE_Y] > total_space[SIZE_Y];

	if (m_scrollbars[SIZE_X])
		total_space[SIZE_Y] -= SCROLLBAR_SIZE; // X-axis needs Y space
	if (m_scrollbars[SIZE_Y])
		total_space[SIZE_X] -= SCROLLBAR_SIZE; // Y-axis needs X space

	DBG_FLEXBOX("==> FlexBox: n=%zu, W % 3i, H % 3i\n",
		m_children.size(), total_space[0], total_space[1]);

	s16 row_y = 0;
	auto it = m_children.begin();
	while (it != m_children.end()) {

		SpreadData xs;
		xs.fixed_space = 0;
		xs.fixed_offset = pos[dir_L] /*- m_scroll_offset[box_axis]*/;
		xs.total_weights = 0;

		auto row_last = it;
		const s16 line_height = getNextLine(xs, row_last);

		if (xs.total_weights == 0)
			xs.total_weights = 1; // avoid division by 0

		// separate leftover space for dynamic placement
		// and fixed space (added incrementally)
		s16 dyn = total_space[box_axis] - xs.fixed_space;
		xs.dynamic_space = std::max<s16>(dyn, 0);
		DBG_FLEXBOX("Row @ n=%li, Y % 3i, Fix % 3i, Dyn % 3i\n",
			row_last - it, row_y, xs.fixed_space, xs.dynamic_space);

		xs.fixed_space = 0;
		xs.weight_sum = 0;

		// Update all positions in the current row
		for (auto e_it = it; e_it != row_last; ++e_it) {
			Element &e = **e_it;

			// Spread along the X axis
			spread(e, xs, box_axis);

			// Spread along the Y axis
			{
				SpreadData ys;
				ys.fixed_space = 0;
				ys.fixed_offset = pos[dir_U] /*- m_scroll_offset[box_axis_O]*/ + row_y;
				ys.total_weights =
					+ e.margin[dir_U]
					+ e.expand[box_axis_O] * 2
					+ e.margin[dir_D];
				ys.weight_sum = 0;

				// (box_axis == SIZE_Y) --> fill X axis
				s16 dyn;
				if (allow_wrap)
					dyn = line_height             - e.min_size[box_axis_O];
				else
					dyn = total_space[box_axis_O] - e.min_size[box_axis_O];
				ys.dynamic_space = std::max<s16>(dyn, 0);

				if (ys.total_weights == 0)
					ys.total_weights = 1; // avoid division by 0

				DBG_FLEXBOX("Y align @ i=%li, Y % 3i, Dyn % 3i\n",
					e_it - m_children.begin(), ys.fixed_offset, ys.dynamic_space);

				spread(e, ys, box_axis_O);
			}
		}
		if (!allow_wrap) {
			// Check whether the position calculation is correct.
			// This only works as long there's enough space for the elements.
			ASSERT_FORCED(xs.fixed_space + xs.dynamic_space * xs.weight_sum
				/ xs.total_weights == total_space[box_axis], "Weights do not sum up");
		}

		row_y += line_height;
		if (row_y > total_space[box_axis_O])
			m_scrollbars[box_axis_O] = true;

		it = row_last;
	}
}

void FlexBox::getMinSize(bool shrink_x, bool shrink_y)
{
	const Size box_axis_O = (box_axis == SIZE_X) ? SIZE_Y : SIZE_X;
	s16_x2 mymin {};

	for (auto &e : m_children) {
		e->getMinSize(shrink_x, shrink_y);
		if (allow_wrap) {
			// Largest element
			mymin[SIZE_X] = std::max<s16>(mymin[SIZE_X], e->min_size[SIZE_X]);
			mymin[SIZE_Y] = std::max<s16>(mymin[SIZE_Y], e->min_size[SIZE_Y]);
		} else {
			// Append to the left (or down)
			mymin[box_axis] += e->min_size[box_axis];
			mymin[box_axis_O] = std::max<s16>(mymin[box_axis_O], e->min_size[box_axis_O]);
		}
	}

	mymin[SIZE_X] += SCROLLBAR_SIZE;
	mymin[SIZE_Y] += SCROLLBAR_SIZE;

	min_size = mymin;
}

s16 FlexBox::getNextLine(SpreadData &d, decltype(m_children)::iterator &it)
{
	const Size box_axis_O = (box_axis == SIZE_X) ? SIZE_Y : SIZE_X;
	const Direction dir_L = (box_axis == SIZE_X) ? DIR_LEFT : DIR_UP;
	const Direction dir_R = (box_axis == SIZE_X) ? DIR_RIGHT : DIR_DOWN;

	s16 total_space = (box_axis == SIZE_X)
		? pos[DIR_RIGHT] - pos[DIR_LEFT]
		: pos[DIR_DOWN] - pos[DIR_UP];
	s16 height_max = 0;

	for (; it != m_children.end(); ++it) {
		const Element &e = **it;
		s16 width = e.min_size[box_axis];

		if (d.fixed_space + width > total_space) {
			if (allow_wrap && height_max > 0)
				break; // wrap here, after adding at least one element to the line

			m_scrollbars[box_axis] = true;
		}

		// weighting of the flexible space
		d.total_weights +=
			+ e.margin[dir_L]
			+ e.expand[box_axis] * 2
			+ e.margin[dir_R];

		// minimum required space
		d.fixed_space += width;
		height_max = std::max<s16>(height_max, e.min_size[box_axis_O]);

		DBG_FLEXBOX("\t line element: W %d, max H %d\n", width, height_max);
	}

	return height_max;
}

/*
|---//M//---[---//P//---MIN_SIZE---//P//---]---//M//---    |

|<-------- total available space for placement ----------->|
*/

void FlexBox::spread(Element &e, SpreadData &d, Size i_width)
{
	Direction dir_L = (i_width == SIZE_X) ? DIR_LEFT : DIR_UP;
	Direction dir_R = (i_width == SIZE_X) ? DIR_RIGHT : DIR_DOWN;

	d.weight_sum += e.margin[dir_L];

	e.pos[dir_L] =
		+ d.fixed_offset // this element
		+ d.fixed_space
		+ d.dynamic_space * d.weight_sum / d.total_weights;

	d.weight_sum += e.expand[i_width] * 2;
	d.fixed_space += e.min_size[i_width];

	e.pos[dir_R] =
		+ d.fixed_offset // this element
		+ d.fixed_space
		+ d.dynamic_space * d.weight_sum / d.total_weights;

	d.weight_sum += e.margin[dir_R];
}

} // namespace guilayout
