#include "unittest_internal.h"
#include "core/blockparams.h"
#include "core/auth.h" // Auth (hash)
#include "core/compressor.h"
#include "core/packet.h"

static const std::string val_str = "Héllo wörld!"; // 14 length
static const int32_t val_s32 = -2035832324;

static void test_blockparams()
{
	BlockParams p1(BlockParams::Type::Text);
	*p1.text = val_str;
	BlockParams p2(BlockParams::Type::Teleporter);
	p2.teleporter.rotation = 0;
	p2.teleporter.id = 10;
	p2.teleporter.dst_id = 42;

	Packet pkt;
	p1.write(pkt);
	p2.write(pkt);

	CHECK(pkt.readStr16() == val_str);
	CHECK(pkt.read<uint8_t>() == 0);
	CHECK(pkt.read<uint8_t>() == 10);
	CHECK(pkt.read<uint8_t>() == 42);
}

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

static void test_view_read_write()
{
	Packet pkt;
	pkt.writeStr16(val_str);

	Packet pkt_w(&pkt);

	pkt.write<int32_t>(0); // Placeholder
	pkt.writeStr16(val_str);

	// Replace placeholder
	pkt_w.write<int32_t>(val_s32 + 42);


	// Read back

	CHECK(pkt.readStr16() == val_str);
	{
		Packet pkt_r(&pkt);
		pkt_r.limitRemainingBytes(4);

		CHECK(pkt_r.read<int32_t>() == val_s32 + 42);
		try {
			pkt_r.read<int32_t>(); // must error
			CHECK(false);
		} catch (std::runtime_error &e) { }
	}
	CHECK(pkt.read<int32_t>() == val_s32 + 42);
	CHECK(pkt.readStr16() == val_str);
}

static void test_compressor()
{
	Packet pkt;
	for (int i = 0; i < 200; ++i) {
		pkt.write<int32_t>(val_s32 + i);
		pkt.writeStr16(val_str);
	}

	unittest_tic();
	Packet pkt_c;
	pkt_c.writeStr16(val_str); // leading payload
	Compressor c(&pkt_c, pkt);
	c.compress();
	pkt_c.write<int32_t>(val_s32); // tailing payload

	Packet pkt_d;
	CHECK(pkt_c.readStr16() == val_str); // leading payload
	Decompressor d(&pkt_d, pkt_c);
	d.decompress();
	CHECK(pkt_c.read<int32_t>() == val_s32); // tailing payload
	unittest_toc("zlib");

	CHECK(pkt.size() == pkt_d.size());

	float ratio = (float)pkt.size() / pkt_c.size();
	printf("zlib compression: normal=%zu, zlib=%zu, ratio=1:%.2f\n", pkt.size(), pkt_c.size(), ratio);

	for (int i = 0; i < 200; ++i) {
		CHECK(pkt_d.read<int32_t>() == val_s32 + i);
		CHECK(pkt_d.readStr16() == val_str);
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
	test_blockparams();
	test_view_read_write();
	test_compressor();
}
