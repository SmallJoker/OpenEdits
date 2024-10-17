#pragma once

enum LogLevel {
	LL_PRINT,
	LL_ERROR,
	LL_WARN,
	LL_INFO,
	LL_DEBUG,
	LL_Invalid
};

class Logger {
public:
	static void doLogStartup();

	Logger(const char *name, LogLevel default_ll = LL_DEBUG);
	inline const char *getName() { return m_name; }

	#ifdef __GNUC__
	#ifdef __MINGW32__
		// https://sourceforge.net/p/mingw-w64/wiki2/gnu%20printf/
		__attribute__((format(gnu_printf, 3, 4)))
	#else
		// "this" is at position 1 !
		__attribute__((format(printf, 3, 4)))
	#endif
	#endif
	void operator()(LogLevel ll, const char *fmt, ...);

	int popErrorCount();

	LogLevel log_level;

private:
	const char *m_name;
	int m_error_count = 0;
};
