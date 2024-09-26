// Re-inventing the wheel

#include "logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#ifndef _WIN32
	#include <unistd.h>
#endif

static bool is_tty = false;
static struct tty_checker {
	tty_checker()
	{
#ifndef _WIN32
		// man isatty 3
		is_tty = isatty(STDOUT_FILENO) && isatty(STDERR_FILENO);
#endif
	}
} run_on_startup;

static time_t startup_time = 0;

void Logger::doLogStartup()
{
	time_t timestamp_now = time(NULL);
	struct tm *tm_info = localtime(&timestamp_now);
	printf("Time: %lu | %s", timestamp_now, asctime(tm_info));

	startup_time = timestamp_now;
}

Logger::Logger(const char *name, LogLevel default_ll) :
	log_level(default_ll),
	m_name(name)
{
}

struct {
	const char *text;
	const char *term_fmt;
} LL_LUT[LL_Invalid] = {
	{ " --- ", "" },
	{ "ERROR", "\e[0;31m" }, // red
	{ " warn", "\e[1;33m" }, // yellow
	{ " info", "\e[1;30m" }, // grey
	{ "debug", "\e[1;30m" }  // grey
};


void Logger::operator()(LogLevel ll, const char *fmt, ...)
{
	unsigned char ll_i = ll;
	if (ll_i > (unsigned char)log_level)
		return;

	FILE *stream = stdout;
	if (ll == LL_ERROR) {
		stream = stderr;
		if (m_error_count < 0x7FFF)
			m_error_count++;
	}

	char buf[1024];

	time_t timestamp_now = time(NULL);
	if (1) {
		fprintf(stream, "%4lu", timestamp_now - startup_time);
	} else {
		// Print current time
		struct tm *tm_info = localtime(&timestamp_now);
		if (strftime(buf, sizeof(buf), "%H:%M:%S", tm_info))
			fputs(buf, stream);
	}

	if (is_tty)
		fputs(LL_LUT[ll_i].term_fmt, stream);

	{
		// Print module name
		fprintf(stream, " %s [%s] ", LL_LUT[ll_i].text, m_name);
	}

	if (is_tty)
		fputs("\e[0m", stream); // reset terminal format

	{
		// Actual log message
		va_list argp;
		va_start(argp, fmt);
		int n = vsnprintf(buf, sizeof(buf) - 1, fmt, argp);
		va_end(argp);
		if (n > 0) {
			if (buf[n - 1] != '\n') {
				buf[n] = '\n';
				buf[n + 1] = '\0';
			}
			fputs(buf, stream);
		}
	}
}

int Logger::popErrorCount()
{
	int cnt = m_error_count;
	m_error_count = 0;
	return cnt;
}
