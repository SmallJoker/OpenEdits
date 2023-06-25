#pragma once

#include "macros.h"
#include <cstdint>
#include <vector>

const uint16_t PROTOCOL_VERSION = 4;
const uint16_t PROTOCOL_VERSION_MIN = 4;
extern size_t CONNECTION_MTU;

namespace std {
	class thread;
}

struct _ENetHost;
struct _ENetPeer;
class Packet;
class PacketProcessor;

class Connection {
public:
	enum ConnectionType {
		TYPE_CLIENT,
		TYPE_SERVER
	};

	Connection(ConnectionType type, const char *name);
	~Connection();
	DISABLE_COPY(Connection)

	bool connect(const char *hostname);

	void flush(); // for debugging
	size_t getPeerIDs(std::vector<peer_t> *fill) const;

	bool listenAsync(PacketProcessor &proc);
	void disconnect(peer_t peer_id);
	std::string getPeerAddress(peer_t peer_id);

	// use only on single connections
	static const peer_t PEER_ID_FIRST = 0;
	enum PacketFlags {
		FLAG_MASK_CHANNEL = 0x00FF, // internal
		FLAG_BROADCAST = 0x0100,
		FLAG_UNRELIABLE = 0x0200,
	};

	void send(peer_t peer_id, uint16_t flags, Packet &pkt);

private:
	static void *recvAsync(void *con);
	void recvAsyncInternal();
	_ENetPeer *findPeer(peer_t peer_id);

	const char *m_name = "";
	std::thread *m_thread = nullptr;
	bool m_running = false;

	std::mutex m_host_lock;
	_ENetHost *m_host = nullptr;

	PacketProcessor *m_processor = nullptr;
};

// abstract
class PacketProcessor {
public:
	virtual void onPeerConnected(peer_t peer_id) {}
	virtual void onPeerDisconnected(peer_t peer_id) {}
	virtual void processPacket(peer_t peer_id, Packet &pkt) = 0;
};
