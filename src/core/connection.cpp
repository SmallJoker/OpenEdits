#include "connection.h"
#include "logger.h"
#include "packet.h"

#include <enet/enet.h>
#include <iostream>
#include <string.h> // strerror
#include <sstream>
#include <thread>

/*
	UDP channel number (CON_CHANNELS) and purpose
	---------------------------------------------

	0 - Default
		Auth, lobby
		World data
		Block updates

	1 - Player-related actions
		Anything related to input/physics/player movement
		Player appearance
		Chat commands (to be adopted)

	Exception: when channel 1 packets hard-depend on world data
	(channel 0), the relevant packets should be sent on channel 0 as well.
*/

static Logger logger("ENet", LL_WARN);

const uint16_t CON_PORT = 0xC014;
const size_t CON_CLIENTS = 32; // server only
const size_t CON_CHANNELS = 2;

// Globally accessible values
const uint16_t PROTOCOL_VERSION_MAX = 9;
const uint16_t PROTOCOL_VERSION_MIN = 7;
// Note: ENet already splits up packets into fragments, thus manual splitting
// for low data volumes should not be necessary.
size_t CONNECTION_MTU;

static struct enet_init {
	enet_init()
	{
		if (enet_initialize() != 0)
			std::terminate();

		puts("--> ENet start");
	}
	~enet_init()
	{
		puts("<-- ENet stop");
		enet_deinitialize();
	}
} ENET_INIT;

Connection::Connection(Connection::ConnectionType type, const char *name)
{
	if (name)
		m_name = name;

	if (type == TYPE_CLIENT) {
		logger(LL_DEBUG, "%s: Initializing\n", m_name);
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

		logger(LL_PRINT, "%s: Starting server on port %d\n", m_name, address.port);
		m_host = enet_host_create(
			&address,
			CON_CLIENTS,
			CON_CHANNELS,
			10 * CON_CLIENTS * 1024, // incoming bandwidth [bytes/s]
			50 * CON_CLIENTS * 1024 // outgoing bandwidth [bytes/s]
		);
	}

	if (!m_host) {
		logger(LL_ERROR, "Failed to initialize instance\n");
	}
}

Connection::~Connection()
{
	m_running = false;
	if (!m_host)
		return;

	for (size_t i = 0; i < m_host->peerCount; ++i)
		enet_peer_disconnect_later(&m_host->peers[i], 0);
	enet_host_flush(m_host);

	if (m_thread) {
		m_thread->join();
		delete m_thread;
	}

	// Apply force if needed
	for (size_t i = 0; i < m_host->peerCount; ++i)
		enet_peer_reset(&m_host->peers[i]);

	enet_host_destroy(m_host);
	logger(LL_DEBUG, "%s: Cleanup done\n", m_name);
}

// -------------- Public members -------------

bool Connection::connect(const char *hostname)
{
	ENetAddress address;
	enet_address_set_host(&address, hostname);
	address.port = CON_PORT;

	ENetPeer *peer = enet_host_connect(m_host, &address, 2, 0);
	if (!peer) {
		logger(LL_ERROR, "%s: No free peers\n", m_name);
		return false;
	}

	char name[100];
	enet_address_get_host(&address, name, sizeof(name));
	logger(LL_PRINT, "%s: Connected to %s:%u\n", m_name, name, address.port);
	return true;
}


void Connection::flush()
{
	enet_host_flush(m_host);
}

size_t Connection::getPeerIDs(std::vector<peer_t> *fill) const
{
	if (fill)
		fill->clear();

	size_t count = 0;
	for (size_t i = 0; i < m_host->peerCount; ++i) {
		if (m_host->peers[i].state == ENET_PEER_STATE_CONNECTED) {
			if (fill)
				fill->push_back(m_host->peers[i].connectID);
			count++;
		}
	}
	return count;
}

bool Connection::listenAsync(PacketProcessor &proc)
{
	if (!m_host)
		return false;

	m_running = true;
	m_processor = &proc;

	m_thread = new std::thread(&recvAsync, this);

	return m_running;
}

void Connection::disconnect(peer_t peer_id)
{
	auto peer = findPeer(peer_id);
	if (!peer)
		return;

	enet_peer_disconnect_later(peer, 0);
	// Actual handling done in async event handler
}

std::string Connection::getPeerAddress(peer_t peer_id)
{
	auto peer = findPeer(peer_id);
	if (!peer)
		return "";

	char buf[30];
	int len = enet_address_get_host(&peer->address, buf, sizeof(buf));
	if (len < 0)
		return "";

	return buf;
}

float Connection::getPeerRTT(peer_t peer_id)
{
	auto peer = findPeer(peer_id);
	if (!peer)
		return 0;

	return peer->roundTripTime * 0.001f; // ms -> s
}

uint32_t Connection::getPeerBytesInTransit(peer_t peer_id)
{
	auto peer = findPeer(peer_id);
	if (!peer)
		return 0;

	return peer->reliableDataInTransit;
}

std::string Connection::getDebugInfo(peer_t peer_id) const
{
	auto peer = findPeer(peer_id);
	if (!peer)
		return "";

	char buf[255];
	snprintf(buf, sizeof(buf),
		"RTT [ms]: last=% 3i, mean=% 3i\n"
		"MTU [bytes]: peer=%i, global=%zu\n"
		"Window size: %i\n"
		"Packet loss: mean=%.0f %%, total=%i\n",
		peer->lastRoundTripTime, peer->roundTripTime,
		peer->mtu, CONNECTION_MTU,
		peer->windowSize,
		(100.f * peer->packetLoss) / ENET_PEER_PACKET_LOSS_SCALE,
		peer->packetsLost
	);

	return std::string(buf);
}

void Connection::send(peer_t peer_id, uint16_t flags, Packet &pkt)
{
	if (pkt.getReadPos() > 0) {
		logger(LL_ERROR, "%s: Tried to send packet that is partially read back\n", m_name);
		return;
	}
	_ENetPacket *epkt = pkt.ptrForSend();

	uint8_t channel = flags & FLAG_MASK_CHANNEL;

	// Most of it should be reliable
	// TODO: test ENET_PACKET_FLAG_UNSEQUENCED
	if (!(flags & FLAG_UNRELIABLE))
		epkt->flags |= ENET_PACKET_FLAG_RELIABLE;

	// Prepare for sending
	SimpleLock lock(m_host_lock);

	if (flags & FLAG_BROADCAST) {
		peer_id = 0;
		enet_host_broadcast(m_host, channel, epkt);
	} else {
		auto peer = findPeer(peer_id);
		if (!peer)
			return;

		peer_id = peer->connectID;
		enet_peer_send(peer, channel, epkt);
	}

	logger(LL_DEBUG, "%s: packet sent. peer_id=%u, channel=%d, dump=%s\n",
		m_name, peer_id, (int)channel, pkt.dump().c_str());
}

// -------------- Private members -------------

void *Connection::recvAsync(void *con_p)
{
	Connection *con = (Connection *)con_p;
	con->recvAsyncInternal();

	logger(LL_PRINT, "%s: Thread stop", con->m_name);
	return nullptr;
}

void Connection::recvAsyncInternal()
{
	int shutdown_seen = 0;
	ENetEvent event;

	std::vector<peer_t> peer_id_list(m_host->peerCount, 0);

	// TODO: Perhaps move async back to sync. This enet stuff is not thread-safe
	while (true) {
		if (!m_running && shutdown_seen == 0) {
			shutdown_seen++;
			logger(LL_DEBUG, "%s: Shutdown requested\n", m_name);

			// Lazy disconnect
			for (size_t i = 0; i < m_host->peerCount; ++i)
				enet_peer_disconnect_later(&m_host->peers[i], 0);
		}

		int status = enet_host_service(m_host, &event, 100);
		if (status < 0) {
			logger(LL_ERROR, "%s: Got host error code %d\n", m_name, status);
		}
		if (status <= 0) {
			// Abort after 500 ms
			if (shutdown_seen > 0) {
				if (++shutdown_seen > 5)
					break;
			}
			continue;
		}

		{
			// Update MTU
			size_t new_mtu = ENET_PROTOCOL_MAXIMUM_MTU;
			for (size_t i = 0; i < m_host->peerCount; ++i) {
				auto &peer = m_host->peers[i];
				if (peer.state != ENET_PEER_STATE_CONNECTED)
					continue;

				if (peer.mtu && peer.mtu < new_mtu) {
					new_mtu = peer.mtu;
				}
			}

			CONNECTION_MTU = new_mtu - 200;
		}

		switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT:
				{
					peer_t peer_id = event.peer->connectID;
					std::string address = getPeerAddress(peer_id);
					logger(LL_DEBUG, "%s: New peer ID %u from %s\n", m_name, peer_id, address.c_str());
					peer_id_list[event.peer - m_host->peers] = peer_id;
					m_processor->onPeerConnected(peer_id);
				}
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				{
					peer_t peer_id = event.peer->connectID;
					logger(LL_DEBUG, "%s: Got packet peer_id=%u, channel=%d, len=%zu\n",
						m_name, peer_id,
						(int)event.channelID,
						event.packet->dataLength
					);

					Packet pkt(&event.packet);
					try {
						m_processor->processPacket(peer_id, pkt);
					} catch (std::exception &e) {
						logger(LL_ERROR, "%s: Unhandled exception while processing packet %s: %s\n",
							m_name, pkt.dump().c_str(), e.what());
					}
				}
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				{
					// event.peer->connectID is always 0. Need to cache it.
					peer_t peer_id = peer_id_list.at(event.peer - m_host->peers);
					logger(LL_DEBUG, "%s: Peer %u disconnected\n", m_name, peer_id);
					m_processor->onPeerDisconnected(peer_id);
				}
				break;
			case ENET_EVENT_TYPE_NONE:
				puts("How did I end up here?");
				break;
		}
	}
}

_ENetPeer *Connection::findPeer(peer_t peer_id) const
{
	// m_host->peers is a simple array. Thread locks are not important.

	for (size_t i = 0; i < m_host->peerCount; ++i) {
		if (m_host->peers[i].state != ENET_PEER_STATE_CONNECTED)
			continue;

		peer_t current = m_host->peers[i].connectID;
		if (current == peer_id || peer_id == PEER_ID_FIRST)
			return &m_host->peers[i];
	}

	return nullptr;
}

