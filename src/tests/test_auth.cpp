#include "unittest_internal.h"
#include "core/auth.h"
#include "server/database_auth.h"


static void auth_friends_test(DatabaseAuth &db)
{
	// Two friends
	{
		AuthAccount auth;
		auth.name = "Buddy";

		CHECK(db.save(auth));
	}
	{
		AuthAccount auth;
		auth.name = "Terry";

		CHECK(db.save(auth));
	}

	{
		// First friend entry
		AuthFriend f;
		f.p1.name = "Buddy";
		f.p2.name = "Terry";

		CHECK(db.setFriend(f));
		// Try a second time
		std::swap(f.p1.name, f.p2.name);
		CHECK(db.setFriend(f));

		std::vector<AuthFriend> friends;
		CHECK(db.listFriends("Terry", &friends));
		CHECK(friends.size() == 1);
		CHECK(friends[0].p1.name == "Terry");
	}

	{
		// Non-existent account
		AuthFriend f;
		f.p1.name = "Terry";
		f.p2.name = "____";

		CHECK(!db.setFriend(f));
	}

	{
		AuthAccount auth;
		auth.name = "Wilson";

		CHECK(db.save(auth));

		AuthFriend f;
		f.p1.name = "Wilson";
		f.p1.status = 420;

		f.p2.name = "Terry";
		f.p2.status = 1;

		CHECK(db.setFriend(f));

		std::vector<AuthFriend> friends;
		db.listFriends("Terry", &friends);
		CHECK(friends.size() == 2);

		// Other side
		db.listFriends("Wilson", &friends);
		CHECK(friends.size() == 1);
		CHECK(friends[0].p1.status == 420);
	}
}

static void auth_database_test()
{
	const char *filepath = "unittest_auth.sqlite3";
	std::remove(filepath); // from failed runs

	DatabaseAuth db;
	CHECK(db.tryOpen(filepath));

	{
		// Register a new account
		AuthAccount auth;
		auth.name = "FooBarBaz";

		Auth newauth;
		newauth.hash(db.getUniqueSalt(), Auth::generatePass());
		auth.password = newauth.output;

		CHECK(db.save(auth));
		CHECK(db.save(auth)); // overwrite

		// By name
		AuthAccount out;
		CHECK(db.load(auth.name, &out));
		CHECK(out.name == auth.name);
	}

	{
		// Configuration test
		AuthConfig cfg;
		cfg.first = "dummy";
		CHECK(!db.getConfig(&cfg));

		cfg.second = "MY VALUE";
		CHECK(db.setConfig(cfg));
		cfg.second.clear();
		CHECK(db.getConfig(&cfg));
		CHECK(cfg.second == "MY VALUE");

		// Delete config
		cfg.second.clear();
		CHECK(db.setConfig(cfg));
		CHECK(!db.getConfig(&cfg));
	}

	auth_friends_test(db);

	db.close();

	std::remove(filepath);
}

void unittest_auth()
{
	const std::string unique_hash = Auth::generateRandom();
	const char *my_salt_1 = "password 123";
	const char *my_salt_2 = "passworb 123";
	{
		// One that passes
		Auth auth_cli, auth_srv;
		auth_cli.hash(unique_hash, my_salt_1);
		CHECK(auth_cli.output.size() > 20);

		auto random = Auth::generateRandom();
		auth_srv.hash(auth_cli.output, random);
		CHECK(auth_srv.output.size() > 20);

		auth_cli.rehash(random);

		CHECK(auth_cli.output == auth_srv.output);
	}

	{
		// Mismatch due to wrong random string
		Auth auth_cli, auth_srv;
		auth_cli.hash(unique_hash, my_salt_1);

		auto random = Auth::generateRandom();
		auth_srv.hash(auth_cli.output, random);

		auth_cli.rehash(my_salt_1); // not the random bytes

		CHECK(auth_cli.output != auth_srv.output);
	}

	{
		// Password mismatch
		Auth auth_cli, auth_srv;
		auth_cli.hash(unique_hash, my_salt_1);
		auth_srv.hash(unique_hash, my_salt_2);

		CHECK(auth_cli.output != auth_srv.output);

		auto random = Auth::generateRandom();
		auth_srv.rehash(random);
		auth_cli.rehash(random);

		CHECK(auth_cli.output != auth_srv.output);
	}

	auth_database_test();
}
