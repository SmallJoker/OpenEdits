#include "database.h"
#include <sqlite3.h>
#include <stdexcept>

Database::~Database()
{
	close();
}

bool Database::tryOpen(const char *filepath)
{
	if (m_database)
		throw std::runtime_error("Tried to open database without closing the previous one");

	m_filename = filepath;
	printf("Database '%s': Opening\n", m_filename.c_str());

	int status = sqlite3_open(m_filename.c_str(), &m_database);
	if (!ok("open", status))
		return false;

	bool good = true;

	good &= ok("begin", sqlite3_prepare_v2(m_database,
		"BEGIN;",
		-1, &m_stmt_begin, nullptr));
	good &= ok("end", sqlite3_prepare_v2(m_database,
		"COMMIT;",
		-1, &m_stmt_end, nullptr));

	return good;
}

void Database::close()
{
	if (!m_database)
		return;

	printf("Database '%s': Closing\n", m_filename.c_str());

	ok("~begin", sqlite3_finalize(m_stmt_begin));
	ok("~end", sqlite3_finalize(m_stmt_end));

	ok("close", sqlite3_close_v2(m_database));
	m_database = nullptr;
}

bool Database::runCustomQuery(const char *query)
{
	if (!m_database)
		return false;

	char *errmsg = nullptr;
	bool good = ok(query, sqlite3_exec(m_database, query, nullptr, nullptr, &errmsg));
	if (!good && errmsg)
		printf("\t Message: %s\n", errmsg);

	return good;
}

bool Database::ok(const char *where, int status)  const
{
	if (status == SQLITE_OK || status == SQLITE_DONE)
		return true;

	printf("Database '%s': '%s' returned error code %d: %s\n",
		m_filename.c_str(), where, status, sqlite3_errmsg(m_database));
	return false;
}
