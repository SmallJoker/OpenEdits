#include "unittest_internal.h"
#include "core/packet.h"

static const std::string val_str = "Héllo wörld!"; // 14 length
static const int32_t val_s32 = -2035832324;

static void test_repeated()
{
	Packet pkt;
	for (int i = 0; i < 200; ++i) {
		pkt.write(val_s32);
		pkt.writeStr16(val_str);
	}
	for (int i = 0; i < 200; ++i) {
		CHECK(pkt.read<int32_t>() == val_s32);
		CHECK(pkt.readStr16() == val_str);
	}
}

void unittest_packet()
{
	const uint8_t val_u8 = 9;
	const uint16_t val_u16 = 44455;
	const float val_f32 = 5.689239658f;

	Packet pkt;
	pkt.writeStr16(val_str);
	pkt.write<uint8_t>(val_u8);
	pkt.write<uint16_t>(val_u16);
	pkt.write<int32_t>(val_s32);
	pkt.writeStr16(val_str);
	pkt.write<float>(val_f32);

	CHECK(pkt.readStr16() == val_str);
	CHECK(pkt.read<uint8_t>() == val_u8);
	CHECK(pkt.read<uint16_t>() == val_u16);
	CHECK(pkt.read<int32_t>() == val_s32);
	CHECK(pkt.readStr16() == val_str);
	CHECK(pkt.read<float>() == val_f32);

	try {
		pkt.read<uint8_t>();
		CHECK(false);
	} catch (std::exception &e) {}

	{
		// Dangerous shorthand for variables
		Packet p2;
		p2.writeStr16(val_str);
		p2.write(val_u8);
		p2.write(val_u16);
		p2.write(val_s32);
		p2.writeStr16(val_str);
		p2.write(val_f32);

		CHECK(p2.size() == pkt.size());
	}

	test_repeated();
}

