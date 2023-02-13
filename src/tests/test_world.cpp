#include "unittest_internal.h"
#include "core/world.h"

void unittest_world()
{
	World w("foobar");
	w.createEmpty({3,5, 0});

	Block b;
	b.id = 9;
	b.param1 = 0;

	CHECK(w.setBlock({2, 2, 0}, b))
	b.id = 0;
	CHECK(w.getBlock({2, 2, 0}, &b))
	CHECK(b.id == 9);

	// Out of range
	CHECK(!w.setBlock({4, 2, 0}, b))

	// Invalid block ID
	b.id = BLOCKID_INVALID;
	CHECK(!w.setBlock({1, 1, 0}, b))

}
