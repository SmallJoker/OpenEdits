#include "remoteplayer.h"

RemotePlayer::RemotePlayer(peer_t peer_id, uint16_t protocol_version) :
	Player(peer_id),
	protocol_version(protocol_version)
{

}
