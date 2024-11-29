#include "database_auth.h"
#include "core/auth.h"
#include <sqlite3.h>
#include <stdexcept> // runtime_error
#include <cstdint>

constexpr int AUTH_DB_VERSION_LATEST = 1;


DatabaseAuth::DatabaseAuth() : Database()
{
	for (size_t i = 0; i < STMT_MAX; ++i)
		m_stmt[i] = nullptr;
}


DatabaseAuth::~DatabaseAuth()
{
	close();
}

bool DatabaseAuth::tryOpen(const char *filepath)
{
	if (!Database::tryOpen(filepath))
		return false;

	if (sqlite3_libversion_number() < 3024000) {
		fprintf(stderr, "Auth DB requires sqlite3 >= 3.24.0 for UPSERT support");
		return false;
	}

	for (size_t i = 0; i < STMT_MAX; ++i) {
		if (!m_stmt[i])
			continue;

		fprintf(stderr, "sqlite statements were not freed properly!");
		return false;
	}

	// Do not execute when failed
#define PREPARE(NAME, STMT) \
	good &= good && ok(#NAME, sqlite3_prepare_v2(m_database, STMT, -1, &m_stmt[NAME], nullptr))

	bool good = true;

	good &= ok("create_cfg", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `config` ("
		"`key`   TEXT UNIQUE,"
		"`value` BLOB,"
		"PRIMARY KEY(`key`)"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(STMT_CFG_READ,
		"SELECT * FROM `config` WHERE key = ? LIMIT 1"
	);
	PREPARE(STMT_CFG_WRITE,
		"REPLACE INTO `config` "
		"(`key`, `value`) "
		"VALUES (?, ?)"
	);
	PREPARE(STMT_CFG_REMOVE,
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

	good &= runCustomQuery("PRAGMA foreign_keys = ON;");

	good &= ok("create_auth", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `auth` ("
		"`name`       TEXT UNIQUE,"
		"`password`   BLOB,"
		"`last_login` INTEGER,"
		"`level`      INTEGER,"
		"`metadata`   BLOB,"
		"PRIMARY KEY(`name`)"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(STMT_AUTH_READ,
		"SELECT * FROM `auth` WHERE `name` = ? LIMIT 1"
	);
	PREPARE(STMT_AUTH_WRITE,
		"INSERT INTO `auth` VALUES(?, ?, ?, ?, ?) "
		"ON CONFLICT(`name`) DO UPDATE SET "
		"	password = excluded.password,"
		"	last_login = excluded.last_login,"
		"	level = excluded.level,"
		"	metadata = excluded.metadata;"
	);
	PREPARE(STMT_AUTH_SET_PW,
		"UPDATE `auth` SET password = ? WHERE name = ?"
	);

	good &= ok("create_friends", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `friends` ("
		"`name1`    TEXT,"
		"`name2`    TEXT,"
		"`status1`   INTEGER,"
		"`status2`   INTEGER,"
		"`metadata` BLOB,"
		"PRIMARY KEY(`name1`, `name2`)"
		// Automatically delete entries when users are removed
		// Also happens when using "REPLACE INTO"
		"CONSTRAINT cs_name1"
		"	FOREIGN KEY (name1)"
		"	REFERENCES auth(name)"
		"		ON DELETE CASCADE,"
		"CONSTRAINT cs_name2"
		"	FOREIGN KEY (name2)"
		"	REFERENCES auth(name)"
		"		ON DELETE CASCADE"
		");",
		nullptr, nullptr, nullptr));
	PREPARE(STMT_FRIENDS_LIST,
		"SELECT * FROM `friends` WHERE name1 = ? OR name2 = ?"
	);
	PREPARE(STMT_FRIENDS_WRITE,
		"INSERT INTO `friends` VALUES(?, ?, ?, ?, ?) "
		"ON CONFLICT(`name1`, `name2`) DO UPDATE SET "
		"	status1 = excluded.status1,"
		"	status2 = excluded.status2,"
		"	metadata = excluded.metadata;"
	);
	PREPARE(STMT_FRIENDS_REMOVE,
		"DELETE FROM `friends` WHERE name1 = ? AND name2 = ?"
	);

	good &= ok("create_f2b", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `fail2ban` ("
		"`expiry`   INTEGER,"
		"`affected` TEXT,"
		"`context`  TEXT,"
		"`comment`  TEXT"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(STMT_F2B_INSERT,
		"INSERT INTO `fail2ban` (`expiry`, `affected`, `context`, `comment`) "
		"VALUES (?, ?, ?, ?)"
	);
	PREPARE(STMT_F2B_READ,
		"SELECT * FROM `fail2ban` WHERE `affected` = ? AND `context` = ? LIMIT 1"
	);
	PREPARE(STMT_F2B_CLEANUP,
		"DELETE FROM `fail2ban` WHERE `expiry` <= ?"
	);

	good &= ok("create_log", sqlite3_exec(m_database,
		"CREATE TABLE IF NOT EXISTS `log` ("
		"`timestamp` INTEGER,"
		"`subject`   TEXT,"
		"`text`      TEXT"
		")",
		nullptr, nullptr, nullptr));

	PREPARE(STMT_LOG_INSERT,
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

	for (size_t i = 0; i < STMT_MAX; ++i) {
		if (!m_stmt[i])
			continue;

		ok("~stmt[]", sqlite3_finalize(m_stmt[i]));
		m_stmt[i] = nullptr;
	}

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

bool DatabaseAuth::getConfig(AuthConfig *entry)
{
	if (!m_database || !entry)
		return false;

	auto s = m_stmt[STMT_CFG_READ];
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

#define WRITE_ACTION(code) \
	sqlite3_step(m_stmt_begin); \
	sqlite3_reset(m_stmt_begin); \
	do \
		code \
	while (0); \
	sqlite3_step(m_stmt_end); \
	sqlite3_reset(m_stmt_end);


bool DatabaseAuth::setConfig(const AuthConfig &entry)
{
	if (!m_database)
		return false;

	bool good = false;

WRITE_ACTION({
	sqlite3_stmt *s;
	if (entry.second.empty()) {
		// Remove
		s = m_stmt[STMT_CFG_REMOVE];
		custom_bind_string(s, 1, entry.first);
	} else {
		// Update
		s = m_stmt[STMT_CFG_WRITE];
		custom_bind_string(s, 1, entry.first);
		custom_bind_blob(s, 2, entry.second);
	}

	good = ok("cfg_write_s", sqlite3_step(s));
	ok("cfg_write_r", sqlite3_reset(s));
})

	return good;
}

bool DatabaseAuth::load(const std::string &name, AuthAccount *auth)
{
	if (!m_database || !auth)
		return false;

	auto s = m_stmt[STMT_AUTH_READ];
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

	bool good = false;

WRITE_ACTION({
	sqlite3_stmt *s = m_stmt[STMT_AUTH_WRITE];
	int i = 1;
	custom_bind_string(s, i++, auth.name);
	custom_bind_blob  (s, i++, auth.password);
	sqlite3_bind_int64(s, i++, auth.last_login);
	sqlite3_bind_int  (s, i++, auth.level);
	custom_bind_blob  (s, i++, ""); // TODO: Use for per-user settings (Packet)

	good = ok("auth_save_s", sqlite3_step(s));
	ok("auth_save_r", sqlite3_reset(s));

	good = good && sqlite3_changes(m_database);

	sqlite3_step(m_stmt_end);
	sqlite3_reset(m_stmt_end);
})

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
		printf("DatabaseAuth: Initializing new server-wide password salt\n");

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

	bool good = false;

WRITE_ACTION({
	auto s = m_stmt[STMT_AUTH_SET_PW];
	int i = 1;
	custom_bind_blob(s, i++, hash);
	custom_bind_string(s, i++, name);

	good = ok("auth_set_pw_s", sqlite3_step(s));
	ok("auth_set_pw_r", sqlite3_reset(s));
})

	return good;
}

bool DatabaseAuth::listFriends(const std::string &name, std::vector<AuthFriend> *friends)
{
	if (!m_database || !friends)
		return false;

	friends->clear();
	auto s = m_stmt[STMT_FRIENDS_LIST];
	custom_bind_string(s, 1, name);
	custom_bind_string(s, 2, name);

	bool good = false;
	while (sqlite3_step(s) == SQLITE_ROW) {
		int i = 0;
		AuthFriend f;

		f.p1.name = (const char *)sqlite3_column_text(s, i++);
		f.p2.name = (const char *)sqlite3_column_text(s, i++);
		f.p1.status = sqlite3_column_int(s, i++);
		f.p2.status = sqlite3_column_int(s, i++);
		//f.metadata = ...

		if (f.p1.name != name)
			std::swap(f.p1, f.p2);

		friends->push_back(std::move(f));
	}

	good = ok("friends_list", sqlite3_errcode(m_database));
	sqlite3_reset(s);

	return good;
}

bool DatabaseAuth::setFriend(AuthFriend f)
{
	if (!m_database)
		return false;

	bool good = false;

	if (f.p1.name > f.p2.name)
		std::swap(f.p1, f.p2);

WRITE_ACTION({
	sqlite3_stmt *s = m_stmt[STMT_FRIENDS_WRITE];
	int i = 1;
	custom_bind_string(s, i++, f.p1.name);
	custom_bind_string(s, i++, f.p2.name);
	sqlite3_bind_int(s, i++, f.p1.status);
	sqlite3_bind_int(s, i++, f.p2.status);
	custom_bind_blob(s, i++, ""); // placeholder

	good = ok("friends_set", sqlite3_step(s));
	sqlite3_reset(s);
})

	return good;
}

bool DatabaseAuth::removeFriend(const std::string &name1, const std::string &name2)
{
	if (!m_database)
		return false;

	bool good = false;

	bool swap = name1 > name2;

WRITE_ACTION({
	auto s = m_stmt[STMT_FRIENDS_REMOVE];
	int i = 1;
	custom_bind_string(s, i++, swap ? name2 : name1);
	custom_bind_string(s, i++, swap ? name1 : name2);

	good = ok("friends_remove", sqlite3_step(s));
	sqlite3_reset(s);
})

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

	auto s = m_stmt[STMT_F2B_INSERT];
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

	auto s = m_stmt[STMT_F2B_READ];
	custom_bind_string(s, 1, affected);
	custom_bind_string(s, 2, context);

	const time_t time_now = time(nullptr);
	bool good = false;
	while (sqlite3_step(s) == SQLITE_ROW) {
		int i = 0;
		int64_t expiry = sqlite3_column_int64(s, i++);
		if (expiry <= time_now)
			continue; // expired

		good = true;
		// Always find the longest lasting ban record.
		if (entry && expiry > entry->expiry) {
			entry->expiry   = expiry;
			entry->affected = (const char *)sqlite3_column_text(s, i++);
			entry->context  = (const char *)sqlite3_column_text(s, i++);
			entry->comment  = (const char *)sqlite3_column_text(s, i++);
		}
	}

	ok("f2b_ban", sqlite3_errcode(m_database));
	sqlite3_reset(s);
	return good;
}

bool DatabaseAuth::cleanupBans()
{
	if (!m_database)
		return false;

	sqlite3_step(m_stmt_begin);
	sqlite3_reset(m_stmt_begin);

	auto s = m_stmt[STMT_F2B_CLEANUP];
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

	auto s = m_stmt[STMT_LOG_INSERT];
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
