#if BUILD_CLIENT

#include "unittest_internal.h"
#include "core/packet.h"
#include "client/clientmedia.h"
#include "server/remoteplayer.h"
#include "server/servermedia.h"


static void test_server_client()
{
	ServerMedia srv;

	srv.indexAssets();
	CHECK(srv.requireAsset("missing_texture.png"));

	RemotePlayer player(420, UINT16_MAX);
	ClientMedia cli;

	{
		Packet pkt;
		srv.writeMediaList(pkt);
		cli.readMediaList(pkt);
	}

	CHECK(cli.countDone() == 1 || cli.countMissing() == 1);
	CHECK((cli.countMissing() == 1) == cli.haveMediaForRequest());

	while (cli.countMissing() > 0) {
		printf("Client: missing %ld files\n", cli.countMissing());
		// Missing file. Cache it.

		if (cli.haveMediaForRequest()) {
			Packet pkt;
			cli.writeMediaRequest(pkt);
			srv.readMediaRequest(&player, pkt);
			CHECK(player.requested_media.size() > 0);
			CHECK(pkt.getRemainingBytes() == 0);
		}

		{
			Packet pkt;
			srv.writeMediaData(&player, pkt);
			cli.readMediaData(pkt);
			CHECK(player.total_sent_media > 0);
			CHECK(pkt.getRemainingBytes() == 0);
		}
	}
	printf("Client: have all %ld files\n", cli.countDone());
}

#include "core/blockmanager.h"
#include "core/script.h"
static void test_with_script()
{
	BlockManager bmgr;
	Script script(&bmgr);

	script.init();
	script.setTestMode("media");
	CHECK(script.loadFromFile("assets/scripts/main.lua"));

}


void unittest_mediamanager()
{
	test_server_client();
	test_with_script();
	puts("---- MediaManager done");
}

#else

void unittest_mediamanager()
{
	puts("MediaManager: test unavilable");
}

#endif
