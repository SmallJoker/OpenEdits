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

constexpr u16 SCROLLBAR_SIZE = 24;

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


void Table::start(const u16_x4 *pos_new)
{
	if (pos_new) {
		pos = *pos_new;

		// TODO: If custom positions are provided,
		// `getMinSize()` may be called multiple times.
		getMinSize(false, false);
		tryFitElements();
	} // else: calculation already done by parent

	if (m_scrollbars[0] || m_scrollbars[1]) {
		// Retry with reduced form
		getMinSize(m_scrollbars[0], m_scrollbars[1]);
		tryFitElements();
	}

	for (auto &e : m_children) {
		if (e)
			e->start(nullptr); // `pos` was updated by `tryFitElements`.
	}
}

void Table::tryFitElements()
{
	u16_x2 total_space {
		(u16)(pos[DIR_RIGHT] - pos[DIR_LEFT]),
		(u16)(pos[DIR_DOWN]  - pos[DIR_UP])
	};

	// reserve space for m_scrollbars
	for (size_t dim = 0; dim < 2; ++dim) {
		Size dim_O = (dim == SIZE_X) ? SIZE_Y : SIZE_X;
		bool have_overflow = total_space[dim] < min_size[dim];
		bool scrollbar_space = total_space[dim_O] > SCROLLBAR_SIZE;

		m_scrollbars[dim] = have_overflow && scrollbar_space;
		if (have_overflow && scrollbar_space)
			total_space[dim_O] -= SCROLLBAR_SIZE;
	}

	// Total space is now known: spread.
	spreadTable(SIZE_X, total_space[SIZE_X]);
	spreadTable(SIZE_Y, total_space[SIZE_Y]);

	// Now center the cell contents
	for (size_t y = 0; y < m_cellinfo[SIZE_Y].size(); ++y)
	for (size_t x = 0; x < m_cellinfo[SIZE_X].size(); ++x) {
		Element *e = m_children[y * m_cellinfo[SIZE_X].size() + x].get();
		if (!e)
			continue;

		spreadCell(e, x, SIZE_X);
		spreadCell(e, y, SIZE_Y);
	}

	// Layout all other boxes if needed
	for (auto &e : m_children) {
		if (!e)
			continue;
		e->tryFitElements();
	}
}

void Table::getMinSize(bool shrink_x, bool shrink_y)
{
	for (size_t dim = 0; dim < 2; ++dim) {
		u16 weight = 0;

		for (CellInfo &ci : m_cellinfo[dim]) {
			ci.min_size = 0;
			weight += ci.weight;
		}
		if (weight == 0)
			weight = 1;

		m_total_weight_cell[dim] = weight;
	}

	for (size_t y = 0; y < m_cellinfo[SIZE_Y].size(); ++y) {
		CellInfo &row = m_cellinfo[SIZE_Y][y];

		for (size_t x = 0; x < m_cellinfo[SIZE_X].size(); ++x) {
			CellInfo &col = m_cellinfo[SIZE_X][x];

			Element *e = m_children[y * m_cellinfo[SIZE_X].size() + x].get();
			if (!e)
				continue;

			e->getMinSize(shrink_x, shrink_y);

			col.min_size = std::max<u16>(col.min_size, e->min_size[0]);
			row.min_size = std::max<u16>(row.min_size, e->min_size[1]);
		}
	}

	// Total "min" size
	u16 fixed_col = 0;
	u16 fixed_row = 0;

	for (CellInfo &ci : m_cellinfo[SIZE_X])
		fixed_col += ci.min_size;
	for (CellInfo &ci : m_cellinfo[SIZE_Y])
		fixed_row += ci.min_size;

	min_size = { fixed_col, fixed_row };
}

void Table::spreadTable(Size dim, u16 total_space)
{
	Direction dir_L = (dim == SIZE_X) ? DIR_LEFT : DIR_UP;

	u16 fixed_offset =
		+ pos[dir_L]
		- m_scroll_offset[dim];
	u16 weight_sum = 0;
	u16 total_springs = m_total_weight_cell[dim];
	int dynamic_space = total_space;

	for (CellInfo &ci : m_cellinfo[dim])
		dynamic_space -= ci.min_size;

	if (dynamic_space < 0)
		dynamic_space = 0;

	for (CellInfo &ci : m_cellinfo[dim]) {
		ci.pos_minmax[0] =
			+ fixed_offset
			+ dynamic_space * weight_sum / total_springs;

		weight_sum += ci.weight;
		fixed_offset += ci.min_size;

		ci.pos_minmax[1] =
			+ fixed_offset
			+ dynamic_space * weight_sum / total_springs;
	}
}

#ifdef DBG_TABLE_LUT
static const char *spread_label[2] = { "-> X", "   Y" };
#endif

void Table::spreadCell(Element *e, u16 num, Size dim)
{
	Direction dir_L = (dim == SIZE_X) ? DIR_LEFT : DIR_UP;
	Direction dir_R = (dim == SIZE_X) ? DIR_RIGHT : DIR_DOWN;

	u16 weight_sum = e->margin[dir_L];
	u16 total_springs =
		+ weight_sum
		+ e->expand[dim] * 2
		+ e->margin[dir_R];
	if (total_springs == 0)
		total_springs = 1;

	CellInfo &ci = m_cellinfo[dim][num];
	u16 fixed_offset = ci.pos_minmax[0];
	u16 fixed_space = e->min_size[dim];
	u16 dynamic_space = ci.pos_minmax[1] - ci.pos_minmax[0] - fixed_space;

	DBG_TABLE("%s% 2i, Min % 4i, Max % 4i, Fix % 3i, Dyn % 4i\n",
			spread_label[dim], num, ci.pos_minmax[0], ci.pos_minmax[1], fixed_space, dynamic_space);

	e->pos[dir_L] =
		+ fixed_offset // static offset
		+ dynamic_space * weight_sum / total_springs;

	weight_sum += e->expand[dim] * 2;
	fixed_offset += fixed_space;

	e->pos[dir_R] =
		+ fixed_offset // static offset
		+ dynamic_space * weight_sum / total_springs;
}


void FlexBox::start(const u16_x4 *pos_new)
{
	if (pos_new) {
		pos = *pos_new;

		getMinSize(false, false);
		tryFitElements();
	} // else: calculation already done by parent

	for (auto &e : m_children)
		e->start(nullptr);
}

#if 0
	#define DBG_BALANCE(...) printf(__VA_ARGS__)
#else
	#define DBG_BALANCE(...) do {} while(0)
#endif

void FlexBox::tryFitElements()
{
	// It is easier to think in 1 direction (X, left to right)
	Direction dir_L = (box_axis == SIZE_X) ? DIR_LEFT : DIR_UP;
	Direction dir_R = (box_axis == SIZE_X) ? DIR_RIGHT : DIR_DOWN;
	Direction dir_U = (box_axis == SIZE_X) ? DIR_UP : DIR_LEFT;
	Direction dir_D = (box_axis == SIZE_X) ? DIR_DOWN : DIR_RIGHT;
	Size box_axis_O = (box_axis == SIZE_X) ? SIZE_Y : SIZE_X;

	m_scroll_offset[SIZE_X] = 0; // maybe preserve?
	m_scroll_offset[SIZE_Y] = 0;

	u16_x2 total_space {
		(u16)(pos[DIR_RIGHT] - pos[DIR_LEFT]),
		(u16)(pos[DIR_DOWN]  - pos[DIR_UP])
	};

	if (m_scrollbars[SIZE_X])
		total_space[SIZE_Y] -= SCROLLBAR_SIZE; // X-axis needs Y space
	if (m_scrollbars[SIZE_Y])
		total_space[SIZE_X] -= SCROLLBAR_SIZE; // Y-axis needs X space

	DBG_BALANCE("==> Balance of n=%zu, W % 3i, H % 3i\n",
		m_children.size(), total_space[0], total_space[1]);

	auto it = m_children.begin();
	while (it != m_children.end()) {

		SpreadData xs;
		xs.fixed_space = 0;
		xs.fixed_offset = pos[dir_L] - m_scroll_offset[box_axis];
		xs.total_springs = 0;
		u16 height_max = 0;

		auto row_last = it;
		u16 row_y = (*it)->m_wrapped_pos;
		for (; row_last != m_children.end(); ++row_last) {
			Element &e = **row_last;

			if (e.m_wrapped_pos != row_y) {
				// deal with this later on
				break;
			}

			// weighting of the flexible space
			// TODO: ONLY ONE PADDING IS NEEDED
			xs.total_springs +=
				+ e.margin[dir_L]
				+ e.expand[box_axis] * 2
				+ e.margin[dir_R];

			// minimum required space
			xs.fixed_space += e.min_size[box_axis];
			height_max = std::max<u16>(height_max, e.min_size[box_axis_O]);
		}

		if (xs.total_springs == 0)
			xs.total_springs = 1; // avoid division by 0

		// separate leftover space for dynamic placement
		// and fixed space (added incrementally)
		int dyn = total_space[box_axis] - xs.fixed_space;
		xs.dynamic_space = std::max<int>(dyn, 0);
		DBG_BALANCE("Row @ n=%li, Y % 3i, Fix % 3i, Dyn % 3i\n",
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
				ys.fixed_offset = pos[dir_U] - m_scroll_offset[box_axis_O] + row_y;
				ys.weight_sum = 0;
				ys.total_springs =
					+ e.margin[dir_U]
					+ e.expand[box_axis_O] * 2
					+ e.margin[dir_D];
				// V-container: fill X axis
				if (allow_wrap)
					dyn = height_max - e.min_size[box_axis_O];
				else
					dyn = total_space[box_axis_O] - e.min_size[box_axis_O];
				ys.dynamic_space = std::max<int>(dyn, 0);

				if (ys.total_springs == 0)
					ys.total_springs = 1; // avoid division by 0

				DBG_BALANCE("Y align @ i=%li, Y % 3i, Dyn % 3i\n",
					e_it - m_children.begin(), ys.fixed_offset, ys.dynamic_space);

				spread(e, ys, box_axis_O);
			}
		}
		if (!allow_wrap) {
			// Check whether the position calculation is correct.
			// This only works as long there's enough space for the elements.
			ASSERT_FORCED(xs.fixed_space + xs.dynamic_space * xs.weight_sum
				/ xs.total_springs == total_space[box_axis], "Weights do not sum up");
		}

		it = row_last;
	}

	// done!

	if (m_scrollbars[SIZE_X])
		min_size[SIZE_Y] += SCROLLBAR_SIZE; // X-axis needs Y space
	if (m_scrollbars[SIZE_Y])
		min_size[SIZE_X] += SCROLLBAR_SIZE; // Y-axis needs X space

	// Layout all other boxes if needed
	for (auto &e : m_children) {
		e->tryFitElements();
	}
}

#if 0
	#define DBG_MIN_SIZE(...) printf(__VA_ARGS__)
#else
	#define DBG_MIN_SIZE(...) do {} while(0)
#endif

void FlexBox::getMinSize(bool shrink_x, bool shrink_y)
{
	// i_width/i_height can be SIZE_X or SIZE_Y
	Size i_width  = box_axis;
	Size i_height = (box_axis == SIZE_X) ? SIZE_Y : SIZE_X;

	// for Y: total height, maximal width
	u16 x_sum = 0,
		y_max = 0;
	u16 x_sum_max = 0;
	u16 y_max_sum = 0;

	// wrapping
	m_scrollbars[SIZE_X] = false;
	m_scrollbars[SIZE_Y] = false;

	u16_x2 total_space {
		(u16)(pos[DIR_RIGHT] - pos[DIR_LEFT]),
		(u16)(pos[DIR_DOWN]  - pos[DIR_UP])
	};
	total_space[SIZE_X] -= SCROLLBAR_SIZE;
	total_space[SIZE_Y] -= SCROLLBAR_SIZE;

	for (auto &ptr : m_children) {
		Element &e = *ptr;
		/*
		// Does not work well with m_scrollbars
		u16_x2 remaining_space;
		remaining_space[i_width] = total_space[i_width] - x_sum;
		remaining_space[i_height] = total_space[i_height] - y_max_sum;
		*/

		e.getMinSize(shrink_x, shrink_y);

		int space = (int)total_space[i_width] - (int)e.min_size[i_width];
		if (space < 0) {
			// entire element is too big -> scrollbar
			m_scrollbars[i_width] = true;
		}
		if (allow_wrap && space < (int)x_sum) {
			DBG_MIN_SIZE("WRAP! X % 3i, space % 3i\n", x_sum, space);

			// Not enough space for this element -> wrap around
			// maximal required width
			x_sum_max = std::max<u16>(x_sum_max, x_sum);
			// total required height
			y_max_sum += y_max;

			x_sum = 0;
			y_max = 0;
		}

		x_sum += e.min_size[i_width];
		y_max = std::max<u16>(y_max, e.min_size[i_height]);

		e.m_wrapped_pos = y_max_sum; // Y offset (DIR_UP)
		DBG_MIN_SIZE("Pointer TOP LEFT @ X % 3i, Y % 3i\n", x_sum, y_max_sum);

		if (y_max_sum + y_max > total_space[i_height]) {
			// not enough Y space due to wrapping -> scollbar
			m_scrollbars[i_height] = true;
		}
	}

	x_sum_max = std::max<u16>(x_sum_max, x_sum);
	y_max_sum += y_max;

	min_size[i_width] = x_sum_max; // maximal width
	min_size[i_height] = y_max_sum; // total height
	DBG_MIN_SIZE("FlexBox min size: W % 3i, H % 3i\n", min_size[0], min_size[1]);
	DBG_MIN_SIZE("\t space: W % 3i, H % 3i\n", total_space[0], total_space[1]);
	DBG_MIN_SIZE("\t scroll: X %i, Y %i\n", m_scrollbars[0], m_scrollbars[1]);

	if (m_scrollbars[SIZE_X])
		min_size[SIZE_Y] += SCROLLBAR_SIZE; // X-axis needs Y space
	if (m_scrollbars[SIZE_Y])
		min_size[SIZE_X] += SCROLLBAR_SIZE; // Y-axis needs X space
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
		+ d.dynamic_space * d.weight_sum / d.total_springs;

	d.weight_sum += e.expand[i_width] * 2;
	d.fixed_space += e.min_size[i_width];

	e.pos[dir_R] =
		+ d.fixed_offset // this element
		+ d.fixed_space
		+ d.dynamic_space * d.weight_sum / d.total_springs;

	d.weight_sum += e.margin[dir_R];
}

} // namespace guilayout
