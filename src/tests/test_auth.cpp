#include "unittest_internal.h"
#include "core/auth.h"
#include "server/database_auth.h"

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

	db.close();

	std::remove(filepath);
}

void unittest_auth()
{
	const std::string unique_hash = Auth::generateRandom();
	{
		// One that passes
		Auth auth_cli, auth_srv;
		auth_cli.hash(unique_hash, "password");
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
		auth_cli.hash(unique_hash, "password");

		auto random = Auth::generateRandom();
		auth_srv.hash(auth_cli.output, random);

		auth_cli.rehash("password"); // not the random bytes

		CHECK(auth_cli.output != auth_srv.output);
	}

	{
		// Password mismatch
		Auth auth_cli, auth_srv;
		auth_cli.hash(unique_hash, "password");
		auth_srv.hash(unique_hash, "password2");

		CHECK(auth_cli.output != auth_srv.output);

		auto random = Auth::generateRandom();
		auth_srv.rehash(random);
		auth_cli.rehash(random);

		CHECK(auth_cli.output != auth_srv.output);
	}

	auth_database_test();
}
