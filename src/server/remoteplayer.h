#pragma once

#include "core/connection.h"
#include <string>

class RemotePlayer {
public:
	const peer_t peer_id;

	std::string name;
	void *world_id;
private:
};
