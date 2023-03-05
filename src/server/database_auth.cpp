#include "database_auth.h"
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
		"`password`       TEXT,"
		"`password_reset` TEXT,"
		"`last_login`     INTEGER,"
		"`resend_retry`   INTEGER,"
		"`ban_expiry`     INTEGER,"
		"`level`          INTEGER,"
		"PRIMARY KEY(`name`)"
		")",
		nullptr, nullptr, nullptr));

	good &= ok("create_log", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `log` ("
		"`timestamp` INTEGER,"
		"`action`    TEXT,"
		"`text`      TEXT"
		")",
		nullptr, nullptr, nullptr));

	good &= ok("read", sqlite3_prepare_v2(m_database,
		"SELECT * FROM `auth` WHERE `name` = ? OR `email` = ? LIMIT 1",
		-1, &m_stmt_read, nullptr));
	good &= ok("write", sqlite3_prepare_v2(m_database,
		"REPLACE INTO `auth` "
		"(`name`, `email`, `password`, `password_reset`, `last_login`, `resend_retry`, `ban_expiry`, `level`) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
		-1, &m_stmt_write, nullptr));
	good &= ok("log", sqlite3_prepare_v2(m_database,
		"REPLACE INTO `log` "
		"(`timestamp`, `action`, `text`) "
		"VALUES (?, ?, ?)",
		-1, &m_stmt_log, nullptr));
	good &= ok("reset_pw", sqlite3_prepare_v2(m_database,
		"UPDATE `auth` SET password_reset = ?, resend_retry = ? WHERE email = ?",
		-1, &m_stmt_reset_pw, nullptr));

	return good;
}

void DatabaseAuth::close()
{
	if (!m_database)
		return;

	ok("~read", sqlite3_finalize(m_stmt_read));
	ok("~write", sqlite3_finalize(m_stmt_write));
	ok("~log", sqlite3_finalize(m_stmt_log));
	ok("~reset_pw", sqlite3_finalize(m_stmt_reset_pw));

	Database::close();
}

static int custom_bind_string(sqlite3_stmt *s, int col, const std::string &text)
{
	return sqlite3_bind_text(s, col, text.c_str(), text.size(), nullptr);
}

bool DatabaseAuth::load(const std::string &what, AuthInformation *auth)
{
	if (!m_database)
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
	auth->password       = (const char *)sqlite3_column_text(s, i++);
	auth->password_reset = (const char *)sqlite3_column_text(s, i++);
	auth->last_login     = sqlite3_column_int64(s, i++);
	auth->resend_retry   = sqlite3_column_int64(s, i++);
	auth->ban_expiry     = sqlite3_column_int64(s, i++);
	auth->level          = sqlite3_column_int(s, i++);

	bool good = ok("read", sqlite3_step(s));
	sqlite3_reset(s);

	return good;
}

bool DatabaseAuth::save(const AuthInformation *auth)
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_write;
	int i = 1;
	custom_bind_string(s, i++, auth->name);
	custom_bind_string(s, i++, auth->email);
	custom_bind_string(s, i++, auth->password);
	custom_bind_string(s, i++, auth->password_reset);
	sqlite3_bind_int64(s, i++, auth->last_login);
	sqlite3_bind_int64(s, i++, auth->resend_retry);
	sqlite3_bind_int64(s, i++, auth->ban_expiry);
	sqlite3_bind_int  (s, i++, auth->level);

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

	std::string newpass = "????";
	// send new password by email

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_reset_pw;
	int i = 1;
	custom_bind_string(s, i++, newpass);
	sqlite3_bind_int64(s, i++, time(nullptr));
	custom_bind_string(s, i++, email);

	bool good = ok("reset_pw_s", sqlite3_step(s));
	ok("reset_pw_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

static_assert(sizeof(time_t) == 8, "Need 64-bit time_t");

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
