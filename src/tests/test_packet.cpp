#include "unittest_internal.h"
#include "core/packet.h"

void unittest_packet()
{
	const uint8_t val_u8 = 9;
	const int32_t val_s32 = -2035832324;
	const float val_f32 = 5.689239658f;
	const std::string val_str = "Héllo\0wörld\0";

	Packet pkt;
	pkt.write<uint8_t>(val_u8);
	pkt.write<int32_t>(val_s32);
	pkt.write<float>(val_f32);
	pkt.writeStr16(val_str);

	CHECK(pkt.read<uint8_t>() == val_u8);
	CHECK(pkt.read<int32_t>() == val_s32);
	CHECK(pkt.read<float>() == val_f32);
	CHECK(pkt.readStr16() == val_str);

	try {
		pkt.read<uint8_t>();
		CHECK(false);
	} catch (std::exception &e) {}

	{
		// Dangerous shorthand for variables
		Packet p2;
		p2.write(val_u8);
		p2.write(val_s32);
		p2.write(val_f32);
		p2.writeStr16(val_str);

		CHECK(p2.size() == pkt.size());
	}
}

