#include "unittest_internal.h"
#include "core/playerflags.h"
#include "core/utils.h"

// Whether the object lifespan outlasts a function call when
// constructed within an argument for the function.
struct LifetimeTest
{
	LifetimeTest()
	{
		v = "dummy value";
		puts("ctor");
	}
	~LifetimeTest()
	{
		puts("dtor");
	}

	const char *get() {
		puts("get()");
		return v.c_str();
	}

	std::string v;
};

static void do_lifetime_test(const char *v)
{
	// must output the value before dtor is called
	puts(v);
}

static void test_playerflags()
{
	using P = PlayerFlags;
	PlayerFlags pf;
	CHECK(!pf.check(P::PF_GODMODE));
	pf.set(P::PF_EDIT_DRAW, 0);
	CHECK(pf.check(P::PF_EDIT));

	pf.flags = P::PF_COOWNER;
	pf.repair();
	CHECK(pf.check(P::PF_EDIT_DRAW));

	PlayerFlags p2;
	p2.flags = P::PF_ADMIN;

	CHECK(!pf.mayManipulate(p2, P::PF_MASK_WORLD));
	CHECK(p2.mayManipulate(pf, P::PF_MASK_WORLD) & P::PF_COOWNER);
}

void unittest_utilities()
{
	const std::string utf8_in1 = "Hello Wörld!";
	const std::wstring utf32_in1 = L"Hello Wörld!";

	std::wstring utf32_out;
	CHECK(utf8_to_wide(utf32_out, utf8_in1.c_str()));
	CHECK(utf32_out == utf32_in1);

	std::string utf8_out;
	CHECK(wide_to_utf8(utf8_out, utf32_in1.c_str()));
	CHECK(utf8_out == utf8_in1);

	const std::string split_in = "foo,,bar   ,baz, more";
	auto parts = strsplit(split_in, ',');
	CHECK(parts.size() == 4);
	CHECK(parts[1] == "bar");
	CHECK(parts[3] == "more");

	do_lifetime_test(LifetimeTest().get());
	test_playerflags();
}
