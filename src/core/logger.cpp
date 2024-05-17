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


Logger::Logger(const char *name, LogLevel default_ll) :
	m_name(name),
	m_level(default_ll)
{
}

struct {
	const char *text;
	const char *term_fmt;
} LL_LUT[LL_Invalid] = {
	{ " --- ", ""},
	{ "ERROR", "\e[0;31m" }, // red
	{ " warn", "\e[1;33m" }, // yellow
	{ "debug", "\e[1;30m" }  // grey
};

void Logger::operator()(LogLevel ll, const char *fmt, ...)
{
	unsigned char ll_i = ll;
	if (ll_i > (unsigned char)m_level)
		return;

	FILE *stream = (ll == LL_ERROR) ? stderr : stdout;

	char buf[1024];

	{
		// Print current time
		time_t timer = time(NULL);
		struct tm *tm_info = localtime(&timer);
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
		int n = vsnprintf(buf, sizeof(buf), fmt, argp);
		va_end(argp);
		if (n > 0)
			fputs(buf, stream);
	}
}
