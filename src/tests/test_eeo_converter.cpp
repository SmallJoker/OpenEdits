#include "unittest_internal.h"
#include "core/eeo_converter.h"
#include "core/world.h"

static void eeoc_write()
{
	World world(g_blockmanager, "eeoc_write");
	auto &meta = world.getMeta();
	world.createEmpty({30, 10});

	// Meta values
	meta.owner = "DUMMYOWNER";
	meta.title = "Some Dummy Title";

	Block b;
	// FG only
	b.id = 9;
	for (u16 x = 4; x <= 20; x++) {
		world.setBlock({x, 4}, b);
		world.setBlock({x, 8}, b);
	}
	// FG + BG
	b.id = 45;
	b.bg = 501;
	world.setBlock({7, 5}, b);

	// BG only
	b.id = 0;
	b.bg = 502;
	for (u16 y = 0; y < 10; y++)
		world.setBlock({25, y}, b);

	EEOconverter conv(world);
	conv.toFile("unittest_1.eelvl");
	std::string relpath = EEOconverter::EXPORT_DIR + "/unittest_1.eelvl";
	EEOconverter::inflate(relpath);

	std::rename(
		relpath.c_str(),
		(EEOconverter::IMPORT_DIR + "/unittest_1.eelvl").c_str()
	);
}

static void eeoc_read_check()
{
	World world(g_blockmanager, "eeoc_read");
	EEOconverter conv(world);
	conv.fromFile("unittest_1.eelvl");

	const auto &meta = world.getMeta();
	CHECK(meta.owner == "DUMMYOWNER");

	blockpos_t size = world.getSize();
	CHECK(size.X == 30);
	CHECK(size.Y == 10);

	Block b;
	CHECK(world.getBlock({12, 8}, &b));
	CHECK(b.id == 9 && b.bg == 0);

	CHECK(world.getBlock({7, 5}, &b));
	CHECK(b.id == 45 && b.bg == 501);

	CHECK(world.getBlock({25, 7}, &b));
	CHECK(b.id == 0 && b.bg == 502);

	std::remove((EEOconverter::IMPORT_DIR + "/unittest_1.eelvl").c_str());
}

void unittest_eeo_converter()
{
	puts("EEOconverter is not available");
	// Compile the functions regardless

	{
		eeoc_write();
		eeoc_read_check();
	}
}
