#include "remoteplayer.h"
#include "core/packet.h"

RemotePlayer::RemotePlayer(peer_t peer_id, uint16_t protocol_version) :
	Player(peer_id),
	protocol_version(protocol_version),
	rl_blocks(70, 2), // 70 blocks per second (draw)
	rl_chat(0.8f, 3), // 1 chat message every 1.25 seconds
	rl_scriptevents(20, 2) // 20 events per second
{

}

Packet RemotePlayer::createPacket(Packet2Client type) const
{
	Packet pkt;
	pkt.data_version = protocol_version;
	pkt.write(type);
	return pkt;
}


void RemotePlayer::runAnticheat(float dtime)
{
	if (cheat_probability < 0)
		return; // disabled

	if (dtime > cheat_probability)
		cheat_probability = 0;
	else
		cheat_probability -= dtime;

	// Parameters reported by the last network packet
	blockpos_t old_bp = getCurrentBlockPos();
	core::vector2df
		old_acc = acc,
		old_vel = vel;

	// Check whether we (the server) have the same physics at
	// this position as the current player.
	// TODO: This step() must use the Lua functions matching the players' protocol version!
	step(0.0001f);

	const float tolerance_sq = 1.1f * 1.1f;
	bool same_bp = (old_bp == getCurrentBlockPos());

	if (dtime > 2.0f) {
		// The client should send an update as soon they move
		// Make sure the penalty does not go overboard
		dtime = 2.0f;
	}

	if (!same_bp) {
		// Different block, thus different physics. Cannot compare.
		// This must be an edge case. Give a penalty so that this gap cannot be abused.
		cheat_probability += 10 * dtime;
	} else if (old_acc.getDistanceFromSQ(acc) > tolerance_sq) {
		cheat_probability += 50 * dtime;
		//printf("acc %f,%f | %f,%f\n", old_acc.X, old_acc.Y, acc.X, acc.Y);
	} else if (old_vel.getDistanceFromSQ(vel) > tolerance_sq) {
		cheat_probability += 25 * dtime;
	}

	// Threshold 1 ( ~200 ?): teleport the player back
	// Threshold 2 ( ~600 ?): kick the player
}
