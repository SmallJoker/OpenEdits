#include "profiler.h"
#include <chrono>
#include <stdio.h>
#include <stdlib.h> // abort
#include <thread>

namespace sc = std::chrono;
using timepoint_t = sc::steady_clock::time_point;

void sleep_ms(long delay)
{
	std::this_thread::sleep_for(sc::milliseconds(delay));
}

TimeTaker::TimeTaker(bool do_start)
{
	(timepoint_t *&)m_start_time = new timepoint_t();

	if (do_start)
		start();
}

TimeTaker::~TimeTaker()
{
	delete (timepoint_t *)m_start_time;
}

void TimeTaker::start()
{
	*(timepoint_t *)m_start_time = sc::steady_clock::now();
}

double TimeTaker::stop()
{
	auto stop_time = sc::steady_clock::now();
	return double {
		sc::duration_cast<sc::duration<double>>
		(stop_time - *(timepoint_t *)m_start_time).count()
	};
}

int64_t TimeTaker::stopMicroseconds()
{
	auto stop_time = sc::steady_clock::now();
	return int64_t {
		sc::duration_cast<sc::microseconds>
		(stop_time - *(timepoint_t *)m_start_time).count()
	};
}


// ------------------  Profiler  ------------------

constexpr size_t PROFILERS_MAX = 10;
static Profiler *profilers[PROFILERS_MAX];
static uint8_t profilers_i = 0;


Profiler::Profiler(const char *name) :
	m_name(name)
{
	if (profilers_i >= PROFILERS_MAX)
		abort();

	profilers[profilers_i] = this;
	profilers_i++;
}

std::string Profiler::get_stats()
{
	char buf[100];
	std::string str;
	str.reserve(1024);

	str.append("Profiler stats:\n");
	for (Profiler *p : profilers) {
		if (!p)
			break;

		snprintf(buf, sizeof(buf), "   %s : calls=% 4d", p->m_name, p->m_sum_calls);
		str.append(buf);

		if (p->m_sum_calls == 0) {
			str.append("\n");
		} else {
			snprintf(buf, sizeof(buf), ", avg=% 4d us\n",
				(int)(p->m_sum_time_us / p->m_sum_calls)
			);
			str.append(buf);
		}
	}
	return str;
}

void Profiler::reset_stats()
{
	for (Profiler *p : profilers) {
		if (!p)
			break;

		p->m_sum_time_us = 0;
		p->m_sum_calls = 0;
	}
}

ScopeProfiler::ScopeProfiler(Profiler &dst) :
	m_profiler(dst),
	m_stopwatch(true)
{
}

ScopeProfiler::~ScopeProfiler()
{
	m_profiler.m_sum_time_us += m_stopwatch.stopMicroseconds();
	m_profiler.m_sum_calls++;
}
