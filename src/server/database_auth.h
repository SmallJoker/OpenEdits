#pragma once

#include "database.h"
#include <ctime>

struct AuthAccount {
	std::string name;
	std::string password;

	time_t last_login = 0;

	enum AccountLevel : int {
		AL_INVALID = 0,
		AL_REGISTERED = 5,
		AL_MODERATOR = 10,
		AL_SERVER_ADMIN = 42
	};
	int level = 0;

	//std::map<std::string, std::string> metadata;
};

using AuthConfig = std::pair<std::string, std::string>;

struct AuthBanEntry {
	time_t expiry = 0;
	std::string affected;
	std::string context;
	std::string comment;
};

struct AuthLogEntry {
	AuthLogEntry()
	{
		timestamp = time(nullptr);
	}

	time_t timestamp;
	std::string subject;
	std::string text;
};

class DatabaseAuth : public Database {
public:
	DatabaseAuth() : Database() {}
	~DatabaseAuth();

	bool tryOpen(const char *filepath) override;
	void close() override;

	// Maybe change this to a Settings object?
	bool getConfig(AuthConfig *entry);
	bool setConfig(const AuthConfig &entry);

	bool load(const std::string &name, AuthAccount *auth);
	bool save(const AuthAccount &auth);

	const std::string &getUniqueSalt();
	bool setPassword(const std::string &name, const std::string &hash);

	bool ban(const AuthBanEntry &entry);
	// returns whether an active ban was found
	bool getBanRecord(const std::string &affected, const std::string &context, AuthBanEntry *entry);
	bool cleanupBans();

	bool logNow(AuthLogEntry entry);


private:
	sqlite3_stmt *m_stmt_cfg_read = nullptr;
	sqlite3_stmt *m_stmt_cfg_write = nullptr;
	sqlite3_stmt *m_stmt_cfg_delete = nullptr;

	sqlite3_stmt *m_stmt_auth_read = nullptr;
	sqlite3_stmt *m_stmt_auth_write = nullptr;
	sqlite3_stmt *m_stmt_auth_set_pw = nullptr;

	sqlite3_stmt *m_stmt_f2b_add = nullptr;
	sqlite3_stmt *m_stmt_f2b_read = nullptr;
	sqlite3_stmt *m_stmt_f2b_cleanup = nullptr;

	sqlite3_stmt *m_stmt_log = nullptr;

	std::string m_unique_salt;
};
