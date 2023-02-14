#include "unittest_internal.h"
#include "core/utils.h"

void unittest_utilities()
{
	const std::string utf8_in1 = "Hello Wörld!";
	const std::wstring utf32_in1 = L"Hello Wörld!";

	std::wstring utf32_out;
	CHECK(utf8_to_utf32(utf32_out, utf8_in1.c_str()));
	CHECK(utf32_out == utf32_in1);

	std::string utf8_out;
	CHECK(utf32_to_utf8(utf8_out, utf32_in1.c_str()));
	CHECK(utf8_out == utf8_in1);
}
