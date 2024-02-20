#include "remoteplayer.h"

RemotePlayer::RemotePlayer(peer_t peer_id, uint16_t protocol_version) :
	Player(peer_id),
	protocol_version(protocol_version),
	rl_blocks(70, 2), // 70 blocks per second (draw)
	rl_chat(0.8f, 3)  // 1 chat message every 1.25 seconds
{

}
