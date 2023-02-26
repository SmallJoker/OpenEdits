#include "unittest_internal.h"
#include "core/world.h"
#include "server/database_world.h"

void unittest_database()
{
	const char *filepath = "unittest.sqlite3";
	DatabaseWorld db;

	CHECK(db.tryOpen(filepath));

	{
		World world(g_blockmanager, "dummyworldname");
		world.createEmpty({5, 2});
		world.getMeta().owner = "test";

		CHECK(db.save(&world));
	}

	{
		World world(g_blockmanager, "dummyworldname");
		CHECK(db.load(&world));
		CHECK(world.getSize().X == 5);
		CHECK(world.getSize().Y == 2);
		CHECK(world.getMeta().owner == "test");
	}

	{
		World world(g_blockmanager, "_does_not_exist_");
		CHECK(!db.load(&world));
	}

	db.close();

	std::remove(filepath);
}

