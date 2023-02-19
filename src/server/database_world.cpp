#include "database_world.h"
#include "core/packet.h"
#include "core/world.h"
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
		"(`id`, `width`, `height`, `owner`, `plays`, `visibility`, `player_flags`, `data`) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
		-1, &m_stmt_write, nullptr));

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

	ok("close", sqlite3_close_v2(m_database));
	m_database = nullptr;

}

static int custom_bind_string(sqlite3_stmt *s, int col, const std::string &text)
{
	return sqlite3_bind_text(s, col, text.c_str(), text.size(), nullptr);
}

static uint32_t stupid_worldid_hash(const std::string &id)
{
	uint32_t v = 0;
	for (char c : id)
		v ^= (v << 3) + (uint8_t)c;
	return v;
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
	meta.plays = sqlite3_column_int(s, 4);
	meta.is_public = sqlite3_column_int(s, 5) == 1;

	{
		// Player flags
		const void *blob = sqlite3_column_blob(s, 6);
		const size_t len = sqlite3_column_bytes(s, 6);
		Packet pkt((const char *)blob, len);
		meta.readPlayerFlags(pkt);
	}

	{
		// World data
		const void *blob = sqlite3_column_blob(s, 7);
		const size_t len = sqlite3_column_bytes(s, 7);
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
	sqlite3_bind_int(s, 5, meta.plays);
	sqlite3_bind_int(s, 6, meta.is_public ? 1 : 0);

	// IMPORTANT: slite3_bind_*(...) does NOT copy the data.
	// The packets must be alive until sqlite3_step(...)
	Packet p_flags;
	meta.writePlayerFlags(p_flags);
	sqlite3_bind_blob(s, 7, p_flags.data(), p_flags.size(), nullptr);

	Packet p_world;
	world->write(p_world, World::Method::Plain);
	sqlite3_bind_blob(s, 8, p_world.data(), p_world.size(), nullptr);

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


bool DatabaseWorld::ok(const char *where, int status)
{
	if (status == SQLITE_OK || status == SQLITE_DONE)
		return true;

	printf("DatabaseWorld: '%s' returned error code %d: %s\n",
		where, status, sqlite3_errmsg(m_database));
	return false;
}

