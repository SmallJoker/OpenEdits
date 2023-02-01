#include "client.h"
#include "core/connection.h"

Client::Client(ClientStartData &init)
{
	m_con = new Connection(Connection::TYPE_CLIENT);
	m_con->connect(init.address.c_str());

	// auth, etc.
}

Client::~Client()
{
	delete m_con;
}
