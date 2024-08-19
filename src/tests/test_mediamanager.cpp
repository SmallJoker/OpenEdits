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
		srv.writeMediaList(&player, pkt);
		cli.readMediaList(pkt);
	}

	CHECK(cli.countDone() == 1 || cli.countMissing() == 1);
	CHECK((cli.countMissing() == 1) == cli.haveMediaForRequest());

	while (cli.countMissing() > 0) {
		printf("Client: missing %zu files\n", cli.countMissing());
		// Missing file. Cache it.

		if (cli.haveMediaForRequest()) {
			Packet pkt;
			cli.writeMediaRequest(pkt);
			srv.readMediaRequest(&player, pkt);
			CHECK(player.media.requested.size() > 0);
			CHECK(pkt.getRemainingBytes() == 0);

			for (const std::string &s : player.media.requested) {
				printf("\t %s\n", s.c_str());
			}
		}

		{
			Packet pkt;
			srv.writeMediaData(&player, pkt);
			cli.readMediaData(pkt);
			CHECK(player.media.total_sent > 0);
			CHECK(pkt.getRemainingBytes() == 0);
		}
	}
	printf("Client: have all %zu files\n", cli.countDone());
}

#include "core/blockmanager.h"
#include "client/clientscript.h"
static void test_with_script()
{
	BlockManager bmgr;
	ClientScript script(&bmgr);

	ClientMedia media;
	media.indexAssets();

	script.init();
	script.setMediaMgr(&media);
	script.setTestMode("media");
	CHECK(script.loadFromAsset("unittest.lua"));
}


void unittest_mediamanager()
{
	test_server_client();
	test_with_script();
	puts("---- MediaManager done");
}

#else

#include <stdio.h>

void unittest_mediamanager()
{
	// We have no Client counterpart to test it
	puts("MediaManager: test unavilable");
}

#endif
