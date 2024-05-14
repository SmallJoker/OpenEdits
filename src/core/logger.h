#pragma once

enum LogLevel {
	LL_PRINT,
	LL_ERROR,
	LL_WARN,
	LL_DEBUG,
	LL_Invalid
};

class Logger {
public:
	Logger(const char *name, LogLevel default_ll = LL_DEBUG);

	#ifdef __GNUC__
		// "this" is at position 1 !
		__attribute__((format(printf, 3, 4)))
	#endif
	void operator()(LogLevel ll, const char *fmt, ...);

private:
	const char *m_name;
	LogLevel m_level;
};
