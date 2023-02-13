#include "unittest_internal.h"
#include "core/world.h"

void unittest_world()
{
	World w("foobar");
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

	// Invalid block ID
	BlockUpdate bu;
	bu.pos = blockpos_t(1, 1);
	bu.id = BLOCKID_INVALID;
	CHECK(!w.updateBlock(bu))

	// Background
	bu.pos = blockpos_t(2, 2);
	bu.id = 501 | BlockUpdate::BG_FLAG;
	CHECK(w.updateBlock(bu));
	CHECK(w.getBlock(bu.pos, &b))
	CHECK(b.id == 9);
	CHECK(b.bg == 501);

	// Remove background
	bu.id = 0 | BlockUpdate::BG_FLAG;
	CHECK(w.updateBlock(bu));
	CHECK(w.getBlock(bu.pos, &b))
	CHECK(b.id == 9);
	CHECK(b.bg == 0);

	// Wrong background
	bu.pos = blockpos_t(1, 2);
	bu.id = 501;
	CHECK(!w.updateBlock(bu));

}
