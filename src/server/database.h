#pragma once

#include <string>

struct sqlite3;
struct sqlite3_stmt;

class Database {
public:
	virtual ~Database();

	virtual bool tryOpen(const char *filepath);
	virtual void close();

	bool runCustomQuery(const char *query);

protected:
	Database() {}

	bool ok(const char *where, int status) const;

	sqlite3 *m_database = nullptr;
	std::string m_filename;

	sqlite3_stmt *m_stmt_begin = nullptr;
	sqlite3_stmt *m_stmt_end = nullptr;
};
