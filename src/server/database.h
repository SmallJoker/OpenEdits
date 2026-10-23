#pragma once

#include <string>

struct sqlite3;
struct sqlite3_stmt;

#define DATABASE_WRITE(code) \
	sqlite3_step(m_stmt_begin); \
	sqlite3_reset(m_stmt_begin); \
	do \
		code \
	while (0); \
	sqlite3_step(m_stmt_end); \
	sqlite3_reset(m_stmt_end);

class Database {
public:
	virtual ~Database();

	virtual bool tryOpen(const char *filepath);
	virtual void close();

	bool runCustomQuery(const char *query);
	bool enableWAL();

protected:
	Database() {}

	bool ok(const char *where, int status) const;

	sqlite3 *m_database = nullptr;
	std::string m_filename;

	sqlite3_stmt *m_stmt_begin = nullptr;
	sqlite3_stmt *m_stmt_end = nullptr;
};
