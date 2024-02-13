#include "environment.h"
#include <chrono>

uint64_t Environment::getTimeNowDIV()
{
	// This is a monstrosity
	namespace sc = std::chrono;
	auto since_epoch = sc::system_clock::now().time_since_epoch();
	auto elapsed = sc::duration_cast<sc::duration<uint64_t, std::ratio<1, TIME_RESOLUTION>>>(since_epoch);

	return elapsed.count();
}
