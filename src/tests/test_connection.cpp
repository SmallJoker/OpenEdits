#include "unittest_internal.h"
#include "core/connection.h"
#include "core/packet.h"

#include <chrono>
#include <thread>

void sleep_ms(long delay)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}


class DummyProcessor : public PacketProcessor {
public:
	void packetProcess(Packet &pkt) override
	{
		printf("... processing packet len=%zu\n", pkt.size());
		last_size = pkt.size();
	}
	size_t last_size = 0;
};

void unittest_connection()
{
	DummyProcessor proc;

	Connection server(Connection::TYPE_SERVER);
	server.listenAsync(proc);

	Connection client(Connection::TYPE_CLIENT);
	client.connect("127.0.0.1");
	client.listenAsync(proc);

	while (client.getPeerCount() != 1)
		sleep_ms(100);

	{
		// From client to server, reliable packet
		Packet pkt;
		pkt.write<int32_t>(3253252);
		pkt.writeStr16("hello world");

		proc.last_size = 0;
		client.send(Connection::PEER_ID_SERVER, 0, pkt);

		while (proc.last_size == 0)
			sleep_ms(100);

		CHECK(proc.last_size == 4 + 2 + 11);
	}
}
