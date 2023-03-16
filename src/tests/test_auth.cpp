#include "unittest_internal.h"
#include "core/auth.h"
#include "server/database_auth.h"

static void auth_database_test()
{
	const char *filepath = "unittest.sqlite3";
	DatabaseAuth db;

	CHECK(db.tryOpen(filepath));

	{
		// Register a new account
		AuthInformation auth;
		auth.name = "FooBarBaz";

		Auth newauth;
		newauth.fromPass(Auth::generatePass());
		auth.password = newauth.getPwHash();

		CHECK(db.save(auth));

		// By name
		 AuthInformation out;
		CHECK(db.load(auth.name, &out));
		CHECK(out.name == auth.name);
	}

	db.close();

	std::remove(filepath);
}

void unittest_auth()
{
	{
		// One that passes
		Auth auth_cli, auth_srv;
		auth_cli.fromPass("password");
		CHECK(auth_cli.getPwHash().size() > 20);

		auth_srv.fromHash(auth_cli.getPwHash());

		auto random = Auth::generateRandom();
		auth_srv.combine(random);
		CHECK(auth_srv.getCombinedHash().size() > 20);

		auth_cli.combine(random);

		CHECK(auth_cli.getCombinedHash() == auth_srv.getCombinedHash());
	}

	{
		// Mismatch due to wrong random string
		Auth auth_cli, auth_srv;
		auth_cli.fromPass("password");

		auth_srv.fromHash(auth_cli.getPwHash());

		auto random = Auth::generateRandom();
		auth_srv.combine(random);

		auth_cli.combine("password"); // not the random bytes

		CHECK(auth_cli.getCombinedHash() != auth_srv.getCombinedHash());
	}

	{
		// Password mismatch
		Auth auth_cli, auth_srv;
		auth_cli.fromPass("password");
		auth_srv.fromPass("password2");

		CHECK(auth_cli.getPwHash() != auth_srv.getPwHash());

		auto random = Auth::generateRandom();
		auth_srv.combine(random);
		auth_cli.combine(random);

		CHECK(auth_cli.getCombinedHash() != auth_srv.getCombinedHash());
	}

	auth_database_test();
}
