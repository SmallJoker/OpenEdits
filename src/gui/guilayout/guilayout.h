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

typedef uint16_t u16;
typedef std::array<u16, 2> u16_x2;
typedef std::array<u16, 4> u16_x4; // TODO: maybe change to s16?

struct Element {
	enum Direction {
		DIR_UP,
		DIR_RIGHT,
		DIR_DOWN,
		DIR_LEFT
	};
	enum Size {
		SIZE_X,
		SIZE_Y
	};

	DISABLE_COPY(Element)

	virtual ~Element() = default;
	virtual void clear() {} // for containers only: drop and clear m_children
	virtual void doRecursive(std::function<bool(Element *)> callback) {}

	u16_x4 margin {}; //< outer springs to space/dock elements (U,R,D,L)
	u16_x2 expand {10, 10}; //< inner springs to enlarge elements (x,y) = padding / 2

	/// modify min_size && return true to skip box calcs
	virtual void getMinSize(bool shrink_x, bool shrink_y) {}
	u16_x2 min_size {}; //< dynamic size (margin, expand) is added atop if available

	/// For the root container: initiates the positioning mechanism
	virtual void start(u16_x2 pos, u16_x2 size) {}
	// For containers
	virtual void tryFitElements() {}
	/// Callback executed after the positions were calculated
	virtual void updatePosition() = 0;
	// Populated after tryFitElements()
	u16_x4 pos {}; //< Minimal and maximal position (1 px overlap with neighbours)

protected:
	friend struct FlexBox; // to access m_wrapped_pos

	Element() = default;

	// Populated after tryFitElements()
	u16 m_wrapped_pos = 0; //< for FlexBox: next line position (X or Y)
};

struct Table : public Element {
	struct CellInfo {
		u16 weight = 10; //< column or row weight for table spreading

	private:
		friend struct Table;
		friend struct IGUIElementWrapper;
		u16 min_size = 0;  // after getMinSize()
		u16_x2 pos_minmax; // {minp, maxp}, after tryFitElements()
	};

	virtual ~Table() = default;

	void clear() override
	{
		m_children.clear();
		m_cellinfo[SIZE_X].clear();
		m_cellinfo[SIZE_Y].clear();
	}

	void doRecursive(std::function<bool(Element *)> callback) override;

	void setSize(u16 x, u16 y)
	{
		m_cellinfo[SIZE_X].resize(x);
		m_cellinfo[SIZE_Y].resize(y);
		m_children.resize(x * y);
	}

	template <typename T, typename ... Args>
	T *add(u16 x, u16 y, Args &&... args)
	{
		checkDimensions(x, y);
		auto &idx = m_children[y * m_cellinfo[SIZE_X].size() + x];
		// Perfect forwarding
		idx = std::make_unique<T>(std::forward<Args>(args)...);
		return dynamic_cast<T *>(idx.get());
	}

	Element *at(u16 x, u16 y) const
	{
		checkDimensions(x, y);
		return m_children[y * m_cellinfo[SIZE_X].size() + x].get();
	}

	CellInfo *col(u16 x)
	{
		checkDimensions(x, SIZE_X);
		return &m_cellinfo[SIZE_X][x];
	}

	CellInfo *row(u16 y)
	{
		checkDimensions(SIZE_X, y);
		return &m_cellinfo[SIZE_Y][y];
	}

	void start(u16_x2 pos, u16_x2 size) override;
	void tryFitElements() override;
	void updatePosition() override;

private:
	friend struct IGUIElementWrapper;

	void checkDimensions(u16 x, u16 y) const
	{
		ASSERT_FORCED(x < m_cellinfo[SIZE_X].size(), "X out of range");
		ASSERT_FORCED(y < m_cellinfo[SIZE_Y].size(), "Y out of range");
	}

	void getMinSize(bool shrink_x, bool shrink_y) override;
	void spreadTable(Size dim, u16 total_space);
	void spreadCell(Element *prim, u16 num, Size dim);

	std::vector<CellInfo> m_cellinfo[2];
	std::vector<std::unique_ptr<Element>> m_children;
	u16 m_total_weight_cell[2]; // after getMinSize()

	bool m_scrollbars[2] {};
	u16 m_scroll_offset[2] {};
};

struct FlexBox : public Element {

	virtual ~FlexBox() = default;

	void clear() override
	{
		m_children.clear();
	}

	void doRecursive(std::function<bool(Element *)> callback) override;

	template <typename T, typename ... Args>
	T *add(Args &&... args)
	{
		m_children.push_back(std::make_unique<T>(std::forward<Args>(args)...));
		return dynamic_cast<T *>(m_children.back().get());
	}

	Element *at(u16 i) const
	{
		return m_children.at(i).get();
	}

	void start(u16_x2 pos, u16_x2 size) override;
	void tryFitElements() override;
	void updatePosition() override;

	Size box_axis = SIZE_X;
	bool allow_wrap = true;

private:
	struct SpreadData {
		u16
			fixed_space,  // reserved by min_size
			fixed_offset, // starting position
			dynamic_space, // for (weight_sum / total_sptrings)
			total_springs,
			weight_sum;
	};

	void getMinSize(bool shrink_x, bool shrink_y) override;
	void spread(Element &box, SpreadData &d, Size i_width);

	std::vector<std::unique_ptr<Element>> m_children;
	bool m_scrollbars[2] {};
	u16 m_scroll_offset[2] {};
};

} // namespace guilayout
