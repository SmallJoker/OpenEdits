#include "database_world.h"
#include "core/packet.h"
#include "core/worldmeta.h"
#include <sqlite3.h>

DatabaseWorld::~DatabaseWorld()
{
	close();
}

bool DatabaseWorld::tryOpen(const char *filepath)
{
	if (!Database::tryOpen(filepath))
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
	good &= ok("featured", sqlite3_prepare_v2(m_database,
		"SELECT `id`, `width`, `height`, `title`, `plays`, `visibility`, `owner`, `data` "
		"FROM `worlds` WHERE `visibility` >= 0 AND length(`data`) > 15 * sqrt(`width` * `height`)",
		-1, &m_stmt_featured, nullptr));

	return good;
}

void DatabaseWorld::close()
{
	if (!m_database)
		return;

	ok("~read", sqlite3_finalize(m_stmt_read));
	ok("~write", sqlite3_finalize(m_stmt_write));
	ok("~by_player", sqlite3_finalize(m_stmt_by_player));
	ok("~featured", sqlite3_finalize(m_stmt_featured));

	Database::close();
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

	meta.type = WorldMeta::Type::Persistent;

	meta.owner = (const char *)sqlite3_column_text(s, 3);
	meta.title = (const char *)sqlite3_column_text(s, 4);
	meta.plays = sqlite3_column_int(s, 5);
	meta.is_public = sqlite3_column_int(s, 6) == 1;

	{
		// Player flags
		const void *blob = sqlite3_column_blob(s, 7);
		const size_t len = sqlite3_column_bytes(s, 7);
		Packet pkt(blob, len);
		meta.readPlayerFlags(pkt);
	}

	{
		// World data
		const void *blob = sqlite3_column_blob(s, 8);
		const size_t len = sqlite3_column_bytes(s, 8);
		Packet pkt(blob, len);
		pkt.data_version = PROTOCOL_VERSION_FAKE_DISK;
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
	p_world.data_version = PROTOCOL_VERSION_FAKE_DISK;
	world->write(p_world, World::Method::Plain);
	sqlite3_bind_blob(s, 9, p_world.data(), p_world.size(), nullptr);

	bool good = ok("save_s", sqlite3_step(s));
	ok("save_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

static void read_world_header(LobbyWorld &meta, sqlite3_stmt *s)
{
	meta.id = (const char *)sqlite3_column_text(s, 0);
	meta.size.X = sqlite3_column_int(s, 1);
	meta.size.Y = sqlite3_column_int(s, 2);
	meta.title = (const char *)sqlite3_column_text(s, 3);
	meta.plays = sqlite3_column_int(s, 4);
	meta.is_public = sqlite3_column_int(s, 5) == 1;
}

std::vector<LobbyWorld> DatabaseWorld::getByPlayer(const std::string &name) const
{
	if (!m_database)
		return {};

	auto s = m_stmt_by_player;
	custom_bind_string(s, 1, name);

	std::vector<LobbyWorld> out;
	while (sqlite3_step(s) == SQLITE_ROW) {
		LobbyWorld meta;
		read_world_header(meta, s);

		meta.owner = name;
		out.emplace_back(meta);
	}

	ok("byPlayer", sqlite3_errcode(m_database));
	sqlite3_reset(s);
	return out;
}

std::vector<LobbyWorld> DatabaseWorld::getFeatured() const
{
	if (!m_database)
		return {};

	auto s = m_stmt_featured;

	std::vector<LobbyWorld> out;
	while (sqlite3_step(s) == SQLITE_ROW) {
		const u8 *data = (u8 *)sqlite3_column_blob(s, 7);
		const size_t length = sqlite3_column_bytes(s, 7);
		// Skip non-compressed worlds (version < 5) --> do a fuzzy check.
		// O E f w METHOD VER CK CK
		if (length < 8 || data[5] < 5)
			continue;

		LobbyWorld meta;
		read_world_header(meta, s);

		meta.owner = (const char *)sqlite3_column_text(s, 6);
		out.emplace_back(meta);
	}

	ok("featured", sqlite3_errcode(m_database));
	sqlite3_reset(s);
	return out;
}
