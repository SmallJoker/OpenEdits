#pragma once

#include <stdint.h> // int64_t
#include <string>

void sleep_ms(long delay);


class TimeTaker {
public:
	TimeTaker(bool do_start);
	~TimeTaker();
	void start();
	/// Returns elapsed time in seconds
	double stop();
	int64_t stopMicroseconds();

private:
	void *m_start_time = nullptr;
};


// ------------------  Profiler  ------------------

class ScopeProfiler;

class Profiler {
public:
	Profiler(const char *name);

	static std::string get_stats();
	static void reset_stats();
private:
	friend class ScopeProfiler;

	const char *m_name;
	int64_t m_sum_time_us;
	int32_t m_sum_calls;
};

class ScopeProfiler {
public:
	ScopeProfiler(Profiler &dst);
	~ScopeProfiler();

private:
	Profiler &m_profiler;
	TimeTaker m_stopwatch;
};
