#pragma once

#include "macros.h"
#include <cstdint>

const uint16_t PROTOCOL_VERSION = 1;
const uint16_t PROTOCOL_VERSION_MIN = 1;

typedef uint32_t peer_t; // same as in ENetPeer

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


	Connection(ConnectionType type);
	~Connection();
	DISABLE_COPY(Connection)

	void connect(const char *hostname);

	void flush(); // for debugging
	size_t getPeerCount() const;

	void listenAsync(PacketProcessor &proc);
	void disconnect(peer_t peer_id);

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
	_ENetPeer *findPeer(peer_t peer_id);

	pthread_t m_thread = 0;
	bool m_running = false;

	_ENetHost *m_host = nullptr;

	std::mutex m_peers_lock;

	std::mutex m_processor_lock;
	PacketProcessor *m_processor = nullptr;
};

// abstract
class PacketProcessor {
public:
	virtual void onPeerConnected(peer_t peer_id) {}
	virtual void onPeerDisconnected(peer_t peer_id) {}
	virtual void processPacket(peer_t peer_id, Packet &pkt) = 0;
};
