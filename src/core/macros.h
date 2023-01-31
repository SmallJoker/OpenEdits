#pragma once

#include <mutex>

#define DISABLE_COPY(T) \
	T(const T &) = delete; \
	T &operator=(const T &) = delete;

#define ASSERT_FORCED(cond, msg) \
	if (!(cond)) { \
		fprintf(stderr, "Assertion ( %s ) failed: %s\n", #cond, msg); \
		exit(EXIT_FAILURE); \
	}

typedef std::unique_lock<std::recursive_mutex> MutexLockR;
typedef std::unique_lock<std::mutex> MutexLock;
