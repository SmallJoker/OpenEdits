#include "unittest_internal.h"
#include <rect.h>

using namespace irr;

static void test_rect()
{
	core::rect<u16> r(UINT16_MAX, UINT16_MAX, 0, 0);
	r.addInternalPoint(4, 7);
	CHECK((int)r.getWidth() * r.getHeight() == 0 * 0);
	r.addInternalPoint(2, 9);
	CHECK((int)r.getWidth() * r.getHeight() == 2 * 2);
}

void unittest_irr()
{
	test_rect();
}
