#include "unittest_internal.h"
#include "core/world.h"

void unittest_world()
{
	World w;
	w.createEmpty({3,5});

	Block b;
	b.id = 9;
	b.param1 = 0;

	CHECK(w.setBlock({2, 2}, b))
	b.id = 0;
	CHECK(w.getBlock({2, 2}, &b))
	CHECK(b.id == 9);

	// Out of range
	CHECK(!w.setBlock({4, 2}, b))

	// Ivalid block ID
	b.id = 0xFFFF;
	CHECK(!w.setBlock({1, 1}, b))

}
