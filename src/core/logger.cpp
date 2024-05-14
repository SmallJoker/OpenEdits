// Re-inventing the wheel

#include "logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

Logger::Logger(const char *name, LogLevel default_ll) :
	m_name(name),
	m_level(default_ll)
{
}

const char *LL_LUT[LL_Invalid] = {
	"print",
	"ERROR",
	"WARN ",
	"debug"
};

void Logger::operator()(LogLevel ll, const char *fmt, ...)
{
	if ((int)ll > (int)m_level)
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

	{
		// Print module name
		fprintf(stream, " %s [%s] ", LL_LUT[(unsigned char)ll], m_name);
	}

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
