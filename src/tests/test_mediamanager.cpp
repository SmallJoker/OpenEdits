#if BUILD_CLIENT

#include "unittest_internal.h"
#include "core/packet.h"
#include "client/clientmedia.h"
#include "server/remoteplayer.h"
#include "server/servermedia.h"


void unittest_mediamanager()
{
	ServerMedia srv;

	srv.indexAssets();
	CHECK(srv.requireMedia("missing_texture.png"));

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

#else

void unittest_mediamanager()
{
	puts("MediaManager: test unavilable");
}

#endif
