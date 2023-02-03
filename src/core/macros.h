#pragma once

#include <mutex>
//#include <shared_mutex>

#define DISABLE_COPY(T) \
	T(const T &) = delete; \
	T &operator=(const T &) = delete;

#define ASSERT_FORCED(cond, msg) \
	if (!(cond)) { throw std::runtime_error(msg); }

typedef std::unique_lock<std::mutex> SimpleLock;
//typedef std::unique_lock<std::shared_mutex> WriteLock;
//typedef std::shared_lock<std::shared_mutex> ReadLock;

typedef uint32_t peer_t; // same as in ENetPeer

