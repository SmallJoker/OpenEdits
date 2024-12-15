#include "unittest_internal.h"
#include "core/packet.h"
#include "core/world.h"

static void test_get_set_update(World &w)
{
	Block b;
	b.id = 9;

	CHECK(w.setBlock({2, 1}, b))
	b.id = 0;
	CHECK(w.getBlock({2, 1}, &b))
	CHECK(b.id == 9);
	CHECK(b.bg == 0);

	// Out of range
	CHECK(!w.setBlock({4, 2}, b))

	// Invalid block ID
	BlockUpdate bu(g_blockmanager);
	bu.pos = blockpos_t(1, 1);
	bu.set(Block::ID_INVALID);
	CHECK(!w.updateBlock(bu))

	// Background on empty foreground
	bu.pos = blockpos_t(0, 2);
	CHECK(bu.set(502));
	CHECK(bu.getId() == 502);
	CHECK(w.updateBlock(bu));
	CHECK(w.getBlock(bu.pos, &b))
	CHECK(b.id == 0);
	CHECK(b.bg == 502);

	// Background on non-empty foreground
	bu.pos = blockpos_t(2, 1);
	CHECK(bu.set(501));
	CHECK(w.updateBlock(bu));
	CHECK(w.getBlock(bu.pos, &b))
	CHECK(b.id == 9);
	CHECK(b.bg == 501);

	// Remove background
	bu.setErase(true);
	CHECK(w.updateBlock(bu));
	CHECK(w.getBlock(bu.pos, &b))
	CHECK(b.id == 9);
	CHECK(b.bg == 0);

	// Out of range
	bid_t newid;
	bool isbg;
	bu.pos = blockpos_t(40, 1);
	bu.set(0);
	CHECK(bu.check(&newid, &isbg));
	CHECK(!w.updateBlock(bu));
}

static void test_readwrite(World &w)
{
	Packet out;
	out.data_version = PROTOCOL_VERSION_FAKE_DISK;
	w.write(out, World::Method::Plain);

	World w2(g_blockmanager, "foobar_check");
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

static void test_positionrange()
{
	World w(g_blockmanager, "foobar_range");
	w.createEmpty({20, 6});

	PositionRange range;
	range.type = PositionRange::T_AREA;

	// Start function
	range.minp = blockpos_t(0, 2);
	range.maxp = blockpos_t(5, 1); // negative Y span
	blockpos_t pos;
	bool ok = range.iteratorStart(&w, &pos);
	CHECK(!ok);

	range.maxp = blockpos_t(10, 6); // x: 0->10, y: 2->6(5)
	ok = range.iteratorStart(&w, &pos);
	CHECK(ok);

	// Iterator function
	CHECK(pos == blockpos_t(0, 2)); // start at minp
	int ok_cnt = 1;
	while (range.iteratorNext(&pos))
		ok_cnt++;

	CHECK(pos == blockpos_t(10, 5)); // end at maxp (limited by world Y)
	CHECK(ok_cnt == ((10 - 0) + 1) * ((5 - 2) + 1)); // area size
}

void unittest_world()
{
	World w(g_blockmanager, "foobar");
	w.createEmpty({3,5});

	test_get_set_update(w);
	test_readwrite(w);
	test_positionrange();
}
