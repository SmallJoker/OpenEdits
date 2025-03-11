#pragma once

#include "core/macros.h"
#include <array>
#include <memory> // unique_ptr
#include <vector>
#include <stddef.h> // size_t
#include <stdint.h> // *int_*
#include <stdio.h> // printf, stderr
#include <functional> // std::function

namespace guilayout {

typedef int16_t s16;
typedef std::array<s16, 2> s16_x2;
typedef std::array<s16, 4> s16_x4;

struct Element {
	enum Direction {
		DIR_LEFT,  // x
		DIR_UP,    // y
		DIR_RIGHT, // x+
		DIR_DOWN   // y+
	};
	enum Size {
		SIZE_X,
		SIZE_Y
	};

	DISABLE_COPY(Element)

	virtual ~Element() = default;

	/// for containers only
	virtual void clear() { m_children.clear(); }
	void doRecursive(std::function<bool(Element *)> callback);

	s16_x4 margin {}; //< outer springs to space/dock elements (L,U,R,D)
	s16_x2 expand {10, 10}; //< inner springs to enlarge elements (x,y) = padding / 2

	/// modify min_size && return true to skip box calcs
	virtual void getMinSize(bool shrink_x, bool shrink_y) {}
	s16_x2 min_size {}; //< dynamic size (margin, expand) is added atop if available

	/// For the current container: initiates the positioning mechanism
	/// `pos_new` must be non-`nullptr` for the root element
	virtual void start(const s16_x4 *pos_new);
	/// Covenience function
	void start(const s16_x4 pos_new) { start(&pos_new); }
	// For containers
	virtual void tryFitElements() {}

	// Populated after tryFitElements()
	s16_x4 pos {}; //< Minimal and maximal position (1 px overlap with neighbours)

protected:
	friend struct FlexBox; // to access m_wrapped_pos

	Element() = default;

	// Populated after getMinSize()
	bool m_scrollbars[2] {}; //< whether the space is exhausted

	// Populated after tryFitElements()
	s16 m_wrapped_pos = 0; //< for FlexBox: next line position (X or Y)

	std::vector<std::unique_ptr<Element>> m_children;
};

struct Table : public Element {
	struct CellInfo {
		s16 weight = 10; //< column or row weight for table spreading

	private:
		friend struct Table;
		friend struct IGUIElementWrapper;
		s16 min_size = 0;  // after getMinSize()
		s16_x2 pos_minmax; // {minp, maxp}, after tryFitElements()
	};

	virtual ~Table() = default;

	void clear() override
	{
		m_children.clear();
		m_cellinfo[SIZE_X].clear();
		m_cellinfo[SIZE_Y].clear();
	}

	void setSize(size_t x, size_t y)
	{
		clear(); // because x/y would no longer match
		m_cellinfo[SIZE_X].resize(x);
		m_cellinfo[SIZE_Y].resize(y);
		m_children.resize(x * y);
	}

	template <typename T, typename ... Args>
	T *add(size_t x, size_t y, Args &&... args)
	{
		checkDimensions(x, y);
		auto &idx = m_children[y * m_cellinfo[SIZE_X].size() + x];
		// Perfect forwarding
		idx = std::make_unique<T>(std::forward<Args>(args)...);
		return dynamic_cast<T *>(idx.get());
	}

	std::unique_ptr<Element> &get(size_t x, size_t y)
	{
		checkDimensions(x, y);
		return m_children[y * m_cellinfo[SIZE_X].size() + x];
	}

	CellInfo *col(size_t x)
	{
		checkDimensions(x, 0);
		return &m_cellinfo[SIZE_X][x];
	}

	CellInfo *row(size_t y)
	{
		checkDimensions(0, y);
		return &m_cellinfo[SIZE_Y][y];
	}

	void tryFitElements() override;

private:
	friend struct IGUIElementWrapper;

	void getMinSize(bool shrink_x, bool shrink_y) override;

	void checkDimensions(size_t x, size_t y) const
	{
		ASSERT_FORCED(x < m_cellinfo[SIZE_X].size(), "X out of range");
		ASSERT_FORCED(y < m_cellinfo[SIZE_Y].size(), "Y out of range");
	}

	void spreadTable(Size dim, s16 total_space);
	void spreadCell(Element *prim, size_t num, Size dim);

	std::vector<CellInfo> m_cellinfo[2];
	s16 m_total_weight_cell[2]; // after getMinSize()
};

struct FlexBox : public Element {

	virtual ~FlexBox() = default;

	template <typename T, typename ... Args>
	T *add(Args &&... args)
	{
		m_children.push_back(std::make_unique<T>(std::forward<Args>(args)...));
		return dynamic_cast<T *>(m_children.back().get());
	}

	Element *at(size_t i) const
	{
		return m_children.at(i).get();
	}

	void tryFitElements() override;

	Size box_axis = SIZE_X;
	bool allow_wrap = true;

private:
	struct SpreadData {
		s16
			fixed_space,   //< == sum(min_size)
			fixed_offset,  //< starting position
			dynamic_space, //< for (weight_sum / total_sptrings)
			total_springs, //< == sum(margin + size + margin)
			weight_sum;
	};

	void getMinSize(bool shrink_x, bool shrink_y) override;
	// Returns the line height
	s16 getNextLine(SpreadData &d, decltype(m_children)::iterator &it);
	void spread(Element &box, SpreadData &d, Size i_width);
};

} // namespace guilayout
