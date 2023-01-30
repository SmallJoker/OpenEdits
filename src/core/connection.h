#pragma once

struct _ENetHost;

class Connection {
public:
	~Connection();

	bool initClient();
	bool initServer();


private:
	_ENetHost *m_con = nullptr;
};
