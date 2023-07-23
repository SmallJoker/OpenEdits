#include "database_auth.h"
#include "core/auth.h"
#include <memory> // unique_ptr
#include <sqlite3.h>
#include <stdexcept> // runtime_error

constexpr int AUTH_DB_VERSION_LATEST = 1;

DatabaseAuth::~DatabaseAuth()
{
	close();
}

bool DatabaseAuth::tryOpen(const char *filepath)
{
	if (!Database::tryOpen(filepath))
		return false;

	// Do not execute when failed
#define PREPARE(NAME, STMT) \
	good &= good && ok(#NAME, sqlite3_prepare_v2(m_database, STMT, -1, &m_stmt_##NAME, nullptr))

	bool good = true;

	good &= ok("create_cfg", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `config` ("
		"`key`   TEXT UNIQUE,"
		"`value` BLOB,"
		"PRIMARY KEY(`key`)"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(cfg_read,
		"SELECT * FROM `config` WHERE key = ? LIMIT 1"
	);
	PREPARE(cfg_write,
		"REPLACE INTO `config` "
		"(`key`, `value`) "
		"VALUES (?, ?)"
	);
	PREPARE(cfg_delete,
		"DELETE FROM `config` WHERE key = ?"
	);

	int version = 0;
	if (!good)
		return false;

	{
		AuthConfig cfg("db.version", "");
		if (getConfig(&cfg)) {
			version = strtol(cfg.second.c_str(), nullptr, 10);
			if (version > AUTH_DB_VERSION_LATEST)
				throw std::runtime_error("Database format is too new for this game version.");
		} else {
			version = AUTH_DB_VERSION_LATEST + 1; // to add the version index
		}
	}


	good &= ok("create_auth", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `auth` ("
		"`name`       TEXT UNIQUE,"
		"`password`   BLOB,"
		"`last_login` INTEGER,"
		"`level`      INTEGER,"
		"`metadata`   TEXT,"
		"PRIMARY KEY(`name`)"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(auth_read,
		"SELECT * FROM `auth` WHERE `name` = ? LIMIT 1"
	);
	PREPARE(auth_write,
		"REPLACE INTO `auth` "
		"(`name`, `password`, `last_login`, `level`, `metadata`) "
		"VALUES (?, ?, ?, ?, ?)"
	);
	PREPARE(auth_set_pw,
		"UPDATE `auth` SET password = ? WHERE name = ?"
	);

	good &= ok("create_f2b", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `fail2ban` ("
		"`expiry`   INTEGER,"
		"`affected` TEXT,"
		"`context`  TEXT,"
		"`comment`  TEXT"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(f2b_add,
		"INSERT INTO `fail2ban` (`expiry`, `affected`, `context`, `comment`) "
		"VALUES (?, ?, ?, ?)"
	);
	PREPARE(f2b_read,
		"SELECT * FROM `fail2ban` WHERE `affected` = ? AND `context` = ? LIMIT 1"
	);
	PREPARE(f2b_cleanup,
		"DELETE FROM `fail2ban` WHERE `expiry` <= ?"
	);

	good &= ok("create_log", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `log` ("
		"`timestamp` INTEGER,"
		"`subject`   TEXT,"
		"`text`      TEXT"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(log,
		"INSERT INTO `log` "
		"(`timestamp`, `subject`, `text`) "
		"VALUES (?, ?, ?)"
	);

#undef PREPARE

	if (good && version != AUTH_DB_VERSION_LATEST) {
		// Assume upgraded
		AuthConfig cfg("db.version", std::to_string(AUTH_DB_VERSION_LATEST));
		setConfig(cfg);
	}
	return good;
}

void DatabaseAuth::close()
{
	if (!m_database)
		return;

#define FINALIZE(NAME) \
	ok("~"#NAME, sqlite3_finalize(m_stmt_##NAME))

	FINALIZE(auth_read);
	FINALIZE(auth_write);
	FINALIZE(auth_set_pw);

	FINALIZE(cfg_read);
	FINALIZE(cfg_write);

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

bool DatabaseAuth::load(const std::string &name, AuthAccount *auth)
{
	if (!m_database || !auth)
		return false;

	auto s = m_stmt_auth_read;
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

	bool good = ok("auth_read", sqlite3_step(s));
	sqlite3_reset(s);

	return good;
}

bool DatabaseAuth::save(const AuthAccount &auth)
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_auth_write;
	int i = 1;
	custom_bind_string(s, i++, auth.name);
	custom_bind_blob  (s, i++, auth.password);
	sqlite3_bind_int64(s, i++, auth.last_login);
	sqlite3_bind_int  (s, i++, auth.level);
	custom_bind_blob(s, i++, ""); // TODO: Use for per-user settings (Packet)

	bool good = ok("auth_save_s", sqlite3_step(s));
	ok("auth_save_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}

bool DatabaseAuth::getConfig(AuthConfig *entry)
{
	if (!m_database || !entry)
		return false;

	auto s = m_stmt_cfg_read;
	custom_bind_string(s, 1, entry->first);

	if (sqlite3_step(s) != SQLITE_ROW) {
		// Not found
		sqlite3_reset(s);
		return false;
	}

	int i = 0;

	entry->first  = (const char *)sqlite3_column_text(s, i++);
	entry->second = custom_column_blob(s, i++);

	bool good = ok("cfg_read", sqlite3_step(s));
	sqlite3_reset(s);

	return good;
}

bool DatabaseAuth::setConfig(const AuthConfig &entry)
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	sqlite3_stmt *s;
	if (entry.second.empty()) {
		// Remove
		s = m_stmt_cfg_delete;
		custom_bind_string(s, 1, entry.first);
	} else {
		// Update
		s = m_stmt_cfg_write;
		custom_bind_string(s, 1, entry.first);
		custom_bind_blob(s, 2, entry.second);
	}

	bool good = ok("cfg_write_s", sqlite3_step(s));
	ok("cfg_write_r", sqlite3_reset(s));

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
	return good;
}


const std::string &DatabaseAuth::getUniqueSalt()
{
	if (!m_unique_salt.empty())
		return m_unique_salt;

	AuthConfig cfg;
	cfg.first = "auth.salt";
	if (!getConfig(&cfg) || cfg.second.empty()) {
		// Create new salt
		printf("DatabaseAuth: Initializing new server-wide password salt");

		cfg.second = Auth::generateRandom();
		if (!setConfig(cfg)) {
			throw std::runtime_error("Failed to save unique salt!");
		}
	}

	m_unique_salt = cfg.second;
	return m_unique_salt;
}


bool DatabaseAuth::setPassword(const std::string &name, const std::string &hash)
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt_auth_set_pw;
	int i = 1;
	custom_bind_blob(s, i++, hash);
	custom_bind_string(s, i++, name);

	bool good = ok("auth_set_pw_s", sqlite3_step(s));
	ok("auth_set_pw_r", sqlite3_reset(s));

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

	bool good = ok("f2b_read", sqlite3_step(s));
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
