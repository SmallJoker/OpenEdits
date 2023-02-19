#include "unittest_internal.h"
#include "core/packet.h"
#include "core/world.h"

static void test_readwrite(World &w)
{
	Packet out;
	w.write(out, World::Method::Plain);

	World w2("foobar_check");
	Packet in(out.data(), out.size());
	w2.createEmpty(w.getSize());
	w2.read(out);

	Block b;
	CHECK(w2.getBlock({2, 1}, &b))
	CHECK(b.id == 9);
	CHECK(b.bg == 0);

	CHECK(w2.getBlock({0, 2}, &b))
	CHECK(b.id == 0);
	CHECK(b.bg == 502);
}

void unittest_world()
{
	World w("foobar");
	w.createEmpty({3,5});

	Block b;
	b.id = 9;
	b.param1 = 0;

	CHECK(w.setBlock({2, 1}, b))
	b.id = 0;
	CHECK(w.getBlock({2, 1}, &b))
	CHECK(b.id == 9);
	CHECK(b.bg == 0);

	// Out of range
	CHECK(!w.setBlock({4, 2}, b))

	// Invalid block ID
	BlockUpdate bu;
	bu.pos = blockpos_t(1, 1);
	bu.id = Block::ID_INVALID;
	CHECK(!w.updateBlock(bu))

	// Background on empty foreground
	bu.pos = blockpos_t(0, 2);
	bu.id = 502 | BlockUpdate::BG_FLAG;
	CHECK(w.updateBlock(bu));
	CHECK(w.getBlock(bu.pos, &b))
	CHECK(b.id == 0);
	CHECK(b.bg == 502);

	// Background on non-empty foreground
	bu.pos = blockpos_t(2, 1);
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


	test_readwrite(w);
}
