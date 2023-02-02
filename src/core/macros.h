#pragma once

#include <mutex>

#define DISABLE_COPY(T) \
	T(const T &) = delete; \
	T &operator=(const T &) = delete;

#define ASSERT_FORCED(cond, msg) \
	if (!(cond)) { throw std::runtime_error(msg); }

typedef std::unique_lock<std::recursive_mutex> MutexLockR;
typedef std::unique_lock<std::mutex> MutexLock;

typedef uint32_t peer_t; // same as in ENetPeer

