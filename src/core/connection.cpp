#include "connection.h"
#include "packet.h"

#include <enet/enet.h>
#include <iostream>
#include <pthread.h>
#include <string.h> // strerror

const uint16_t CON_PORT = 0xC014;
const size_t CON_CLIENTS = 32; // server only
const size_t CON_CHANNELS = 2;

struct enet_init {
	enet_init()
	{
		if (enet_initialize() != 0) {
			fprintf(stderr, "An error occurred while initializing ENet.\n");
			exit(EXIT_FAILURE);
		}
		puts("--> ENet start");
	}
	~enet_init()
	{
		puts("<-- ENet stop");
		enet_deinitialize();
	}
} ENET_INIT;

Connection::Connection(Connection::ConnectionType type)
{
	if (type == TYPE_CLIENT) {
		printf("--- ENet: Initializing client\n");
		m_host = enet_host_create(
			NULL,
			1, // == server
			CON_CHANNELS,
			0, // unlimited incoming bandwidth
			0 // unlimited outgoing bandwidth
		);
	}

	if (type == TYPE_SERVER) {
		ENetAddress address;
		address.host = ENET_HOST_ANY;
		address.port = CON_PORT;

		printf("--- ENet: Starting server on port %d\n", address.port);
		m_host = enet_host_create(
			&address,
			CON_CLIENTS,
			CON_CHANNELS,
			0, // unlimited incoming bandwidth
			0 // unlimited outgoing bandwidth
		);
	}

	if (!m_host) {
		fprintf(stderr, "-!- ENet: Failed to initialite instance\n");
	}
}

Connection::~Connection()
{
	m_running = false;

	if (m_thread) {
		pthread_join(m_thread, nullptr);
		m_thread = 0;
	}

	// Apply force if needed
	for (size_t i = 0; i < m_host->peerCount; ++i)
		enet_peer_reset(&m_host->peers[i]);

	enet_host_destroy(m_host);
}

// -------------- Public members -------------

void Connection::connect(const char *hostname)
{
	ENetAddress address;
	enet_address_set_host(&address, hostname);
	address.port = CON_PORT;

	ENetPeer *peer = enet_host_connect(m_host, &address, 2, 0);
	if (!peer) {
		fprintf(stderr, "-!- ENet: No free peers\n");
		return;
	}

	char name[100];
	enet_address_get_host(&address, name, sizeof(name));
	printf("--- ENet: Connected to %s:%u\n", name, address.port);
}


void Connection::flush()
{
	enet_host_flush(m_host);
}

size_t Connection::getPeerCount() const
{
	size_t count = 0;
	for (size_t i = 0; i < m_host->peerCount; ++i) {
		if (m_host->peers[i].state == ENET_PEER_STATE_CONNECTED)
			count++;
	}
	return count;
}

void Connection::listenAsync(PacketProcessor &proc)
{
	m_running = true;

	int status = pthread_create(&m_thread, nullptr, &recvAsync, this);

	if (status != 0) {
		m_running = false;
		fprintf(stderr, "-!- ENet: Failed to start pthread: %s\n", strerror(status));
		return;
	}

	m_processor = &proc;
}

void Connection::disconnect(peer_t peer_id)
{
	auto peer = findPeer(peer_id);
	enet_peer_disconnect(peer, 0);
	// Actual handling done in async event handler
}

void Connection::send(peer_t peer_id, uint16_t flags, Packet &pkt)
{
	auto peer = findPeer(peer_id);
	uint8_t channel = flags & FLAG_MASK_CHANNEL;

	// Most of it should be reliable
	// TODO: test ENET_PACKET_FLAG_UNSEQUENCED
	if (!(flags & FLAG_UNRELIABLE))
		pkt.data()->flags |= ENET_PACKET_FLAG_RELIABLE;

	if (flags & FLAG_BROADCAST)
		enet_host_broadcast(m_host, channel, pkt.data());
	else
		enet_peer_send(peer, channel, pkt.data());

	printf("--- ENet: packet sent. peer_id=%u, len=%zu\n", peer->connectID, pkt.size());
}

// -------------- Private members -------------

void *Connection::recvAsync(void *con_p)
{
	Connection *con = (Connection *)con_p;

	int shutdown_seen = 0;
	ENetEvent event;

	while (true) {
		if (!con->m_running && shutdown_seen == 0) {
			shutdown_seen++;
			printf("--- ENet: Shutdown requested\n");

			// Lazy disconnect
			MutexLock lock(con->m_peers_lock);
			for (size_t i = 0; i < con->m_host->peerCount; ++i)
				enet_peer_disconnect_later(&con->m_host->peers[i], 0);
		}

		if (enet_host_service(con->m_host, &event, 1000) <= 0) {
			// Abort after 2 * 1000 ms
			if (shutdown_seen > 0) {
				if (++shutdown_seen > 2)
					break;
			}

			continue;
		}

		switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT:
				{
					printf("--- ENet: New peer ID %u\n", event.peer->connectID);
				}
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				{
					printf("--- ENet: Got packet peer_id=%u, channel=%d, len=%zu\n",
						event.peer->connectID,
						(int)event.channelID,
						event.packet->dataLength
					);

					Packet pkt(&event.packet);
					MutexLock lock(con->m_processor_lock);
					con->m_processor->packetProcess(pkt);
				}
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				{
					// event.peer->connectID is always 0
					printf("--- ENet: Peer disconnected\n");
				}
				break;
			case ENET_EVENT_TYPE_NONE:
				puts("How did I end up here?");
				break;
		}
	}

	printf("<-- ENet: Thread stop\n");
	return nullptr;
}


_ENetPeer *Connection::findPeer(peer_t peer_id)
{
	MutexLock lock(m_peers_lock);

	for (size_t i = 0; i < m_host->peerCount; ++i) {
		peer_t current = m_host->peers[i].connectID;
		if (current == peer_id || peer_id == PEER_ID_SERVER)
			return &m_host->peers[i];
	}

	std::string v("Cannot find Peer ID ");
	v.append(std::to_string(peer_id));
	throw std::runtime_error(v);
}

