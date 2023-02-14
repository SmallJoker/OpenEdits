#include "unittest_internal.h"
#include "core/world.h"
#include "server/database_world.h"

void unittest_database()
{
	DatabaseWorld db;

	CHECK(db.tryOpen("unittest.sqlite3"));

	{
		World world("dummyworldname");
		world.createEmpty({30, 20});
		world.getMeta().owner = "test";

		CHECK(db.save(&world));
	}

	{
		World world("dummyworldname");
		CHECK(db.load(&world));
		CHECK(world.getSize().X == 30);
		CHECK(world.getSize().Y == 20);
		CHECK(world.getMeta().owner == "test");
	}
	db.close();
}

