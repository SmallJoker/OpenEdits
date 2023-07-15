#include "database_auth.h"
#include "core/auth.h"
#include <memory> // unique_ptr
#include <sqlite3.h>
#include <stdexcept> // runtime_error

DatabaseAuth::~DatabaseAuth()
{
	close();
}

bool DatabaseAuth::tryOpen(const char *filepath)
{
	if (!Database::tryOpen(filepath))
		return false;

	bool good = ok("create_auth", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `auth` ("
		"`name`       TEXT UNIQUE,"
		"`password`   BLOB,"
		"`last_login` INTEGER,"
		"`level`      INTEGER,"
		"`metadata`   TEXT,"
		"PRIMARY KEY(`name`)"
		")",
		nullptr, nullptr, nullptr));

	good &= ok("create_f2b", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `fail2ban` ("
		"`expiry`   INTEGER,"
		"`affected` TEXT,"
		"`context`  TEXT,"
		"`comment`  TEXT"
		")",
		nullptr, nullptr, nullptr));

	good &= ok("create_log", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `log` ("
		"`timestamp` INTEGER,"
		"`subject`   TEXT,"
		"`text`      TEXT"
		")",
		nullptr, nullptr, nullptr));

#define PREPARE(NAME, STMT) \
	good &= ok(#NAME, sqlite3_prepare_v2(m_database, STMT, -1, &m_stmt_##NAME, nullptr))

	PREPARE(read,
		"SELECT * FROM `auth` WHERE `name` = ? LIMIT 1"
	);
	PREPARE(write,
		"REPLACE INTO `auth` "
		"(`name`, `password`, `last_login`, `level`, `metadata`) "
		"VALUES (?, ?, ?, ?, ?)"
	);
	PREPARE(set_pw,
		"UPDATE `auth` SET password = ? WHERE name = ?"
	);

	PREPARE(f2b_add,
		"REPLACE INTO `fail2ban` (`expiry`, `affected`, `context`, `comment`) "
		"VALUES (?, ?, ?, ?)"
	);
	PREPARE(f2b_read,
		"SELECT * FROM `fail2ban` WHERE `affected` = ? AND `context` = ? LIMIT 1"
	);
	PREPARE(f2b_cleanup,
		"DELETE FROM `fail2ban` WHERE `expiry` <= ?"
	);

	PREPARE(log,
		"REPLACE INTO `log` "
		"(`timestamp`, `subject`, `text`) "
		"VALUES (?, ?, ?)"
	);

	return good;
}

void DatabaseAuth::close()
{
	if (!m_database)
		return;

#define FINALIZE(NAME) \
	ok("~"#NAME, sqlite3_finalize(m_stmt_##NAME))

	FINALIZE(read);
	FINALIZE(write);
	FINALIZE(set_pw);

	FINALIZE(f2b_add);
	FINALIZE(f2b_read);
	FINALIZE(f2b_cleanup);

	FINALIZE(log);

	Database::close();
}

static int custom_bind_string(sqlite3_stmt *s, int col, const std::string &text)
{
	return sqlite3_bind_text(s, col, text.c_str(), text.size(), nullptr);
}

static int custom_bind_blob(sqlite3_stmt *s, int col, const std::string &text)
{
	return sqlite3_bind_blob(s, col, text.c_str(), text.size(), nullptr);
}

static std::string custom_column_blob(sqlite3_stmt *s, int col)
{
	const void *blob = sqlite3_column_blob(s, col);
	const size_t len = sqlite3_column_bytes(s, col);
	return std::string((const char *)blob, len);
}

bool DatabaseAuth::load(const std::string &name, AuthInformation *auth)
{
	if (!m_database || !auth)
		return false;

	auto s = m_stmt_read;
	custom_bind_string(s, 1, name);

	if (sqlite3_step(s) != SQLITE_ROW) {
		// Not found
		sqlite3_reset(s);
		return false;
	}

	int i = 0;

	auth->name           = (const char *)sqlite3_column_text(s, i++);
	auth->password       = custom_column_blob(s, i++);
	auth->last_login     = sqlite3_column_int64(s, i++);
	auth->level          = sqlite3_column_int(s, i++);
	auth->metadata.clear(); // TODO: Yet not implemented.

	bool good = ok("read", sqlite3_step(s));
	sqlite3_reset(s);

	return good;
}

bool DatabaseAuth::save(const AuthInformation &auth)
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_write;
	int i = 1;
	custom_bind_string(s, i++, auth.name);
	custom_bind_blob(s, i++, auth.password);
	sqlite3_bind_int64(s, i++, auth.last_login);
	sqlite3_bind_int  (s, i++, auth.level);
	custom_bind_blob(s, i++, ""); // TODO: Yet not implemented.

	bool good = ok("save_s", sqlite3_step(s));
	ok("save_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

const std::string &DatabaseAuth::getUniqueSalt()
{
	if (!m_unique_salt.empty())
		return m_unique_salt;

	AuthInformation info;
	info.name = "\x1B$AUTH_SETTINGS";
	if (!load(info.name, &info) || info.password.empty()) {
		// Create new salt
		printf("DatabaseAuth: Initializing new server-wide password salt");

		info.password = Auth::generateRandom();
		info.last_login = INT64_MAX; // prevent accidental removal by cleanups
		if (!save(info)) {
			throw std::runtime_error("Failed to save unique salt!");
		}
	}
	m_unique_salt = info.password;
	return m_unique_salt;
}


bool DatabaseAuth::setPassword(const std::string &name, const std::string &hash)
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_set_pw;
	int i = 1;
	custom_bind_blob(s, i++, hash);
	custom_bind_string(s, i++, name);

	bool good = ok("set_pw_s", sqlite3_step(s));
	ok("set_pw_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

static_assert(sizeof(time_t) == 8, "Need 64-bit time_t");

bool DatabaseAuth::ban(const AuthBanEntry &entry)
{
	if (!m_database)
		return false;

	if (entry.expiry <= time(nullptr)) {
		fprintf(stderr, "DatabaseAuth: Invalid expiry date for ban. affected='%s', context='%s'\n",
			entry.affected.c_str(), entry.context.c_str());
		return false;
	}

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_f2b_add;
	int i = 1;
	sqlite3_bind_int64(s, i++, entry.expiry);
	custom_bind_string(s, i++, entry.affected);
	custom_bind_string(s, i++, entry.context);
	custom_bind_string(s, i++, entry.comment);

	bool good = ok("f2b_s", sqlite3_step(s));
	ok("f2b_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);

	if (good) {
		printf("Server: Banned %s (context='%s') until %llu\n",
			entry.affected.c_str(), entry.context.c_str(), (unsigned long long)entry.expiry
		);
	}

	return good;
}

bool DatabaseAuth::getBanRecord(const std::string &affected, const std::string &context, AuthBanEntry *entry)
{
	if (!m_database)
		return false;

	auto s = m_stmt_f2b_read;
	custom_bind_string(s, 1, affected);
	custom_bind_string(s, 2, context);

	if (sqlite3_step(s) != SQLITE_ROW) {
		// Not found
		sqlite3_reset(s);
		return false;
	}

	int i = 0;
	int64_t expiry = sqlite3_column_int64(s, i++);

	if (entry) {
		entry->expiry   = expiry;
		entry->affected = (const char *)sqlite3_column_text(s, i++);
		entry->context  = (const char *)sqlite3_column_text(s, i++);
		entry->comment  = (const char *)sqlite3_column_text(s, i++);
	}

	bool good = ok("read f2b", sqlite3_step(s));
	sqlite3_reset(s);

	return good && expiry > time(nullptr);
}

bool DatabaseAuth::cleanupBans()
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_f2b_cleanup;
	int i = 1;
	sqlite3_bind_int64(s, i++, time(nullptr));

	bool good = ok("f2b_cleanup_s", sqlite3_step(s));
	ok("f2b_cleanup_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

bool DatabaseAuth::logNow(AuthLogEntry entry)
{
	entry.timestamp = time(nullptr);

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_log;
	int i = 1;
	sqlite3_bind_int64(s, i++, entry.timestamp);
	custom_bind_string(s, i++, entry.subject);
	custom_bind_string(s, i++, entry.text);

	bool good = ok("log_s", sqlite3_step(s));
	ok("log_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}
