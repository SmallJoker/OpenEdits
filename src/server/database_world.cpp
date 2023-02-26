#include "database_world.h"
#include "core/packet.h"
#include <sqlite3.h>

DatabaseWorld::~DatabaseWorld()
{
	if (m_database)
		close();
}

bool DatabaseWorld::tryOpen(const char *filepath)
{
	printf("DatabaseWorld: Opening database %s\n", filepath);

	int status = sqlite3_open(filepath, &m_database);
	if (!ok("open", status))
		return false;

	// Thanks "DB Browser for SQLite"
	bool good = ok("create", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `worlds` ("
		"`id`     TEXT UNIQUE,"
		"`width`  INTEGER,"
		"`height` INTEGER,"
		"`owner`  TEXT,"
		"`title`  TEXT,"
		"`plays`  INTEGER,"
		"`visibility`   INTEGER,"
		"`player_flags` BLOB,"
		"`data`   BLOB,"
		"PRIMARY KEY(`id`)"
		")",
		nullptr, nullptr, nullptr));

	good &= ok("begin", sqlite3_prepare_v2(m_database,
		"BEGIN;",
		-1, &m_stmt_begin, nullptr));
	good &= ok("end", sqlite3_prepare_v2(m_database,
		"COMMIT;",
		-1, &m_stmt_end, nullptr));
	good &= ok("read", sqlite3_prepare_v2(m_database,
		"SELECT * FROM `worlds` WHERE `id` = ? LIMIT 1",
		-1, &m_stmt_read, nullptr));
	good &= ok("write", sqlite3_prepare_v2(m_database,
		"REPLACE INTO `worlds` "
		"(`id`, `width`, `height`, `owner`, `title`, `plays`, `visibility`, `player_flags`, `data`) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
		-1, &m_stmt_write, nullptr));
	good &= ok("by_player", sqlite3_prepare_v2(m_database,
		"SELECT `id`, `width`, `height`, `title`, `plays`, `visibility` "
		"FROM `worlds` WHERE `owner` = ?",
		-1, &m_stmt_by_player, nullptr));

	return good;
}

void DatabaseWorld::close()
{
	if (!m_database)
		return;

	printf("DatabaseWorld: Closing database\n");

	ok("~begim", sqlite3_finalize(m_stmt_begin));
	ok("~end", sqlite3_finalize(m_stmt_end));
	ok("~read", sqlite3_finalize(m_stmt_read));
	ok("~write", sqlite3_finalize(m_stmt_write));
	ok("~by_player", sqlite3_finalize(m_stmt_by_player));

	ok("close", sqlite3_close_v2(m_database));
	m_database = nullptr;

}

static int custom_bind_string(sqlite3_stmt *s, int col, const std::string &text)
{
	return sqlite3_bind_text(s, col, text.c_str(), text.size(), nullptr);
}

bool DatabaseWorld::load(World *world)
{
	if (!m_database)
		return false;

	// Check if it exists
	// Use a read-only transaction

	SimpleLock lock(world->mutex);

	WorldMeta &meta = world->getMeta();

	auto s = m_stmt_read;
	custom_bind_string(s, 1, meta.id);

	if (sqlite3_step(s) != SQLITE_ROW) {
		// Not found
		sqlite3_reset(s);
		return false;
	}

	blockpos_t size;
	size.X = sqlite3_column_int(s, 1);
	size.Y = sqlite3_column_int(s, 2);
	world->createDummy(size);

	meta.owner = (const char *)sqlite3_column_text(s, 3);
	meta.title = (const char *)sqlite3_column_text(s, 4);
	meta.plays = sqlite3_column_int(s, 5);
	meta.is_public = sqlite3_column_int(s, 6) == 1;

	{
		// Player flags
		const void *blob = sqlite3_column_blob(s, 7);
		const size_t len = sqlite3_column_bytes(s, 7);
		Packet pkt((const char *)blob, len);
		meta.readPlayerFlags(pkt);
	}

	{
		// World data
		const void *blob = sqlite3_column_blob(s, 8);
		const size_t len = sqlite3_column_bytes(s, 8);
		Packet pkt((const char *)blob, len);
		world->read(pkt);
	}

	sqlite3_step(s);
	bool good = ok("read", sqlite3_errcode(m_database));
	sqlite3_reset(s);

	return good;
}

bool DatabaseWorld::save(const World *world)
{
	if (!m_database)
		return false;

	SimpleLock lock(world->mutex);

	// https://www.sqlite.org/lang_transaction.html
	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	const auto &meta = world->getMeta();

	auto s = m_stmt_write;
	custom_bind_string(s, 1, meta.id);
	sqlite3_bind_int(s, 2, world->getSize().X);
	sqlite3_bind_int(s, 3, world->getSize().Y);
	custom_bind_string(s, 4, meta.owner);
	custom_bind_string(s, 5, meta.title);
	sqlite3_bind_int(s, 6, meta.plays);
	sqlite3_bind_int(s, 7, meta.is_public ? 1 : 0);

	// IMPORTANT: slite3_bind_*(...) does NOT copy the data.
	// The packets must be alive until sqlite3_step(...)
	Packet p_flags;
	meta.writePlayerFlags(p_flags);
	sqlite3_bind_blob(s, 8, p_flags.data(), p_flags.size(), nullptr);

	Packet p_world;
	world->write(p_world, World::Method::Plain, UINT16_MAX);
	sqlite3_bind_blob(s, 9, p_world.data(), p_world.size(), nullptr);

	bool good = ok("save_s", sqlite3_step(s));
	ok("save_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

bool DatabaseWorld::runCustomQuery(const char *query)
{
	if (!m_database)
		return false;

	char *errmsg = nullptr;
	bool good = ok(query, sqlite3_exec(m_database, query, nullptr, nullptr, &errmsg));
	if (!good && errmsg)
		printf("\t Message: %s\n", errmsg);

	return good;
}

std::vector<LobbyWorld> DatabaseWorld::getByPlayer(const std::string &name) const
{
	std::vector<LobbyWorld> out;
	if (!m_database)
		return out;

	auto s = m_stmt_by_player;
	custom_bind_string(s, 1, name);

	while (sqlite3_step(s) == SQLITE_ROW) {
		std::string world_id = (const char *)sqlite3_column_text(s, 0);

		LobbyWorld meta(world_id);
		meta.size.X = sqlite3_column_int(s, 1);
		meta.size.Y = sqlite3_column_int(s, 2);
		meta.title = sqlite3_column_int(s, 3);
		meta.owner = name;
		meta.plays = sqlite3_column_int(s, 4);
		meta.is_public = sqlite3_column_int(s, 5) == 1;

		out.emplace_back(meta);
	}

	ok("byPlayer", sqlite3_errcode(m_database));
	sqlite3_reset(s);
	return out;
}


bool DatabaseWorld::ok(const char *where, int status)  const
{
	if (status == SQLITE_OK || status == SQLITE_DONE)
		return true;

	printf("DatabaseWorld: '%s' returned error code %d: %s\n",
		where, status, sqlite3_errmsg(m_database));
	return false;
}

