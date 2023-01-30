#include "connection.h"
#include <iostream>
#include <enet/enet.h>

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

Connection::~Connection()
{
	enet_host_destroy(m_con);
}

bool Connection::initServer()
{
	ENetAddress address;
	address.host = ENET_HOST_ANY;
	address.port = 0xC014;

	m_con = enet_host_create(
		&address,
		32, // 32 clients
		2, // 2 channels ,
		0, // unlimited incoming bandwidth
		0 // unlimited outgoing bandwidth
	);

	if (!m_con) {
		fprintf(stderr, "An error occurred while trying to create an ENet server host.\n");
	}

	return !!m_con;
}

bool Connection::initClient()
{
	m_con = enet_host_create(
		NULL,
		1, // 32 clients
		2, // 2 channels ,
		0, // unlimited incoming bandwidth
		0 // unlimited outgoing bandwidth
	);

	if (!m_con) {
		fprintf(stderr, "An error occurred while trying to create an ENet client host.\n");
	}

	return !!m_con;
}
