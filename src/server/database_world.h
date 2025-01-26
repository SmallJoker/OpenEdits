#pragma once

#include "database.h"
#include "core/world.h" // LobbyWorld
#include <vector>

class DatabaseWorld : Database {
public:
	DatabaseWorld() : Database() {}
	~DatabaseWorld();

	bool tryOpen(const char *filepath) override;
	void close() override;

	bool load(World *world);
	bool save(const World *world);

	std::vector<LobbyWorld> getByPlayer(const std::string &name) const;

private:
	sqlite3_stmt *m_stmt_read = nullptr;
	sqlite3_stmt *m_stmt_write = nullptr;

	sqlite3_stmt *m_stmt_by_player = nullptr;
};
