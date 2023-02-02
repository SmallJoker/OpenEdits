#include "unittest_internal.h"
#include "core/connection.h"
#include "core/packet.h"

void sleep_ms(long delay);


class DummyProcessor : public PacketProcessor {
public:
	void processPacket(peer_t peer_id, Packet &pkt) override
	{
		printf("... processing packet %s\n", pkt.dump().c_str());
		last_size = pkt.size();
	}
	size_t last_size = 0;
};

void unittest_connection()
{
	DummyProcessor proc;

	Connection server(Connection::TYPE_SERVER, "TestServer");
	server.listenAsync(proc);

	Connection client(Connection::TYPE_CLIENT, "TestClient");
	client.connect("127.0.0.1");
	client.listenAsync(proc);

	while (client.getPeerIDs(nullptr) != 1)
		sleep_ms(100);

	{
		// From client to server, reliable packet
		Packet pkt;
		pkt.write<int32_t>(0x50EFBE); // BEEFS
		pkt.writeStr16("hello world");

		for (int i = 0; i < 2; ++i) {
			// Attempt to re-send (e.g. for another peer...?)

			proc.last_size = 0;
			client.send(0, 0, pkt);

			while (proc.last_size == 0)
				sleep_ms(100);

			CHECK(proc.last_size == 4 + 2 + 11);
		}
	}

	{
		// Broadcast to clients on channel 1
		Packet pkt;
		pkt.writeStr16("test");

		proc.last_size = 0;
		server.send(0, 1 | Connection::FLAG_BROADCAST, pkt);

		while (proc.last_size == 0)
			sleep_ms(100);

		CHECK(proc.last_size == 2 + 4);
	}
}
