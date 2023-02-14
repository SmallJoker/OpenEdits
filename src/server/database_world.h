#pragma once

class World;
struct sqlite3;
struct sqlite3_stmt;

class DatabaseWorld {
public:
	~DatabaseWorld();

	bool tryOpen(const char *filepath);
	void close();

	bool load(World *world);
	bool save(const World *world);

	bool runCustomQuery(const char *query);

private:
	bool ok(const char *where, int status);

	sqlite3 *m_database = nullptr;

	sqlite3_stmt *m_stmt_begin = nullptr;
	sqlite3_stmt *m_stmt_end = nullptr;

	sqlite3_stmt *m_stmt_read = nullptr;
	sqlite3_stmt *m_stmt_write = nullptr;
};
