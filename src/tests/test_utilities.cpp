#include "unittest_internal.h"
#include "core/playerflags.h"
#include "core/timer.h"
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

	{
		PlayerFlags p3;
		p3.repair();
		CHECK(p3.flags == 0);
		printf("Role of p3: flags=%08X, str=%s\n", p3.flags, p3.toHumanReadable().c_str());
	}

	// Interact check
	PlayerFlags pf;
	CHECK(!pf.check(P::PF_GODMODE));
	pf.set(P::PF_EDIT_DRAW, 0);
	CHECK(pf.check(P::PF_EDIT));

	pf.set(P::PF_COOWNER, P::PF_MASK_WORLD);
	CHECK(pf.check(P::PF_EDIT_DRAW | P::PF_COOWNER));

	PlayerFlags p2(P::PF_ADMIN);

	CHECK(!pf.mayManipulate(p2, P::PF_MASK_WORLD));
	CHECK(p2.mayManipulate(pf, P::PF_MASK_WORLD) & P::PF_COOWNER);

	pf.repair();
	printf("Role of p1: flags=%08X, str=%s\n", pf.flags, pf.toHumanReadable().c_str());

	p2.repair();
	printf("Role of p2: flags=%08X, str=%s\n", p2.flags, p2.toHumanReadable().c_str());
}

static void test_timer()
{
	Timer t;
	CHECK(!t.isActive());

	// overwrite with 3s cooldown
	t.set(3.0f);
	CHECK(!t.set(-2.0f)); // do not refill
	t.step(2.9f);
	CHECK(t.isActive());

	CHECK(t.step(0.2f)); // timer stopped
	CHECK(!t.isActive());

	CHECK(t.set(-2.0f)); // allow refill
	CHECK(t.isActive());
	CHECK(t.step(2.2f)); // timer stopped
}

static void test_rate_limit()
{
	RateLimit rl(1 / 20.0f, 2);
	CHECK(!rl.isActive());
	rl.add(39);
	CHECK(!rl.isActive());
	rl.add(2);
	CHECK(rl.isActive());

	rl.step(1.0f); // 1 second passes (-20)
	CHECK(!rl.isActive());
	rl.add(21);
	CHECK(rl.isActive());
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
	test_timer();
	test_rate_limit();
}
