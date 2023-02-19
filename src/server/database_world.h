#pragma once

#include "core/world.h" // WorldMeta
#include <vector>

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

	std::vector<LobbyWorld> getByPlayer(const std::string &name) const;

private:
	bool ok(const char *where, int status) const;

	sqlite3 *m_database = nullptr;

	sqlite3_stmt *m_stmt_begin = nullptr;
	sqlite3_stmt *m_stmt_end = nullptr;

	sqlite3_stmt *m_stmt_read = nullptr;
	sqlite3_stmt *m_stmt_write = nullptr;

	sqlite3_stmt *m_stmt_by_player = nullptr;
};
