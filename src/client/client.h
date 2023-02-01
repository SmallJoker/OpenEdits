#pragma once

#include <string>

class Connection;

struct ClientStartData {
	std::string address;
	std::string nickname;
};

// Abstract for inheritance
class Client {
public:
	Client(ClientStartData &init);
	~Client();

protected:
	Connection *m_con = nullptr;
};
