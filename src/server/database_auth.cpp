#include "database_auth.h"
#include "core/auth.h"
#include <sqlite3.h>

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
		"`name`           TEXT UNIQUE,"
		"`email`          TEXT,"
		"`password`       BLOB,"
		"`password_reset` BLOB,"
		"`last_login`     INTEGER,"
		"`level`          INTEGER,"
		"PRIMARY KEY(`name`)"
		")",
		nullptr, nullptr, nullptr));

	good &= ok("create_f2b", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `fail2ban` ("
		"`expiry` INTEGER,"
		"`what`   TEXT,"
		"`reason` TEXT"
		")",
		nullptr, nullptr, nullptr));

	good &= ok("create_log", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `log` ("
		"`timestamp` INTEGER,"
		"`action`    TEXT,"
		"`text`      TEXT"
		")",
		nullptr, nullptr, nullptr));

#define PREPARE(NAME, STMT) \
	good &= ok(#NAME, sqlite3_prepare_v2(m_database, STMT, -1, &m_stmt_##NAME, nullptr))

	PREPARE(read,
		"SELECT * FROM `auth` WHERE `name` = ? OR `email` = ? LIMIT 1"
	);
	PREPARE(write,
		"REPLACE INTO `auth` "
		"(`name`, `email`, `password`, `password_reset`, `last_login`,`level`) "
		"VALUES (?, ?, ?, ?, ?, ?)"
	);
	PREPARE(reset_pw,
		"UPDATE `auth` SET password_reset = ? WHERE email = ?"
	);

	PREPARE(f2b_add,
		"REPLACE INTO `fail2ban` (`expiry`, `what`, `reason`) "
		"VALUES (?, ?, ?)"
	);
	PREPARE(f2b_read,
		"SELECT * FROM `fail2ban` WHERE `what` = ? LIMIT 1"
	);
	PREPARE(f2b_cleanup,
		"DELETE FROM `fail2ban` WHERE `expiry` < ?"
	);

	PREPARE(log,
		"REPLACE INTO `log` "
		"(`timestamp`, `action`, `text`) "
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
	FINALIZE(reset_pw);

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

bool DatabaseAuth::load(const std::string &what, AuthInformation *auth)
{
	if (!m_database || !auth)
		return false;

	auto s = m_stmt_read;
	custom_bind_string(s, 1, what);
	custom_bind_string(s, 2, what);

	if (sqlite3_step(s) != SQLITE_ROW) {
		// Not found
		sqlite3_reset(s);
		return false;
	}

	int i = 0;

	auth->name           = (const char *)sqlite3_column_text(s, i++);
	auth->email          = (const char *)sqlite3_column_text(s, i++);
	auth->password       = custom_column_blob(s, i++);
	auth->password_reset = custom_column_blob(s, i++);
	auth->last_login     = sqlite3_column_int64(s, i++);
	auth->level          = sqlite3_column_int(s, i++);

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
	custom_bind_string(s, i++, auth.email);
	custom_bind_blob(s, i++, auth.password);
	custom_bind_blob(s, i++, auth.password_reset);
	sqlite3_bind_int64(s, i++, auth.last_login);
	sqlite3_bind_int  (s, i++, auth.level);

	bool good = ok("save_s", sqlite3_step(s));
	ok("save_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

bool DatabaseAuth::resetPassword(const std::string &email)
{
	if (!m_database)
		return false;

	std::string newpass = Auth::generatePass();

	Auth newauth;
	newauth.fromPass(newpass);

	// send new password by email
	printf("Generated password %s\n", newpass.c_str());

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_reset_pw;
	int i = 1;
	custom_bind_blob(s, i++, newauth.getPwHash());
	custom_bind_string(s, i++, email);

	bool good = ok("reset_pw_s", sqlite3_step(s));
	ok("reset_pw_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

static_assert(sizeof(time_t) == 8, "Need 64-bit time_t");

bool DatabaseAuth::ban(const AuthBanEntry &entry)
{
	if (!m_database)
		return false;

	if (entry.expiry <= time(nullptr))
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_log;
	int i = 1;
	sqlite3_bind_int64(s, i++, entry.expiry);
	custom_bind_string(s, i++, entry.what);
	custom_bind_string(s, i++, entry.reason);

	bool good = ok("f2b_s", sqlite3_step(s));
	ok("f2b_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);

	if (good)
		printf("Banned %s until %lu\n", entry.what.c_str(), entry.expiry);

	return good;
}

bool DatabaseAuth::getBanRecord(const std::string &what, AuthBanEntry *entry)
{
	if (!m_database || !entry)
		return false;

	auto s = m_stmt_f2b_read;
	custom_bind_string(s, 1, what);

	if (sqlite3_step(s) != SQLITE_ROW) {
		// Not found
		sqlite3_reset(s);
		return false;
	}

	int i = 0;

	entry->expiry = sqlite3_column_int64(s, i++);
	entry->what   = (const char *)sqlite3_column_text(s, i++);
	entry->reason = (const char *)sqlite3_column_text(s, i++);

	bool good = ok("read f2b", sqlite3_step(s));
	sqlite3_reset(s);

	return good && entry->expiry > time(nullptr);
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
	custom_bind_string(s, i++, entry.action);
	custom_bind_string(s, i++, entry.text);

	bool good = ok("log_s", sqlite3_step(s));
	ok("log_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}
