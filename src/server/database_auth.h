#pragma once

#include "database.h"

struct AuthInformation {
	std::string name;
	std::string email;
	std::string password;
	std::string password_reset;

	time_t last_login = 0;

	enum AccountLevel : int {
		AL_INVALID = 0,
		AL_REGISTERED = 5,
		AL_ADMIN = 10
	};
	int level = 0;
};

struct AuthBanEntry {
	time_t expiry = 0;
	std::string what;
	std::string reason;
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

class DatabaseAuth : Database {
public:
	DatabaseAuth() : Database() {}
	~DatabaseAuth();

	bool tryOpen(const char *filepath) override;
	void close() override;

	bool load(const std::string &what, AuthInformation *auth);
	bool save(const AuthInformation &auth);

	bool resetPassword(const std::string &email);

	bool ban(const AuthBanEntry &entry);
	// returns whether an active ban was found
	bool getBanRecord(const std::string &what, AuthBanEntry *entry);
	bool cleanupBans();

	bool logNow(AuthLogEntry entry);

private:
	sqlite3_stmt *m_stmt_read = nullptr;
	sqlite3_stmt *m_stmt_write = nullptr;
	sqlite3_stmt *m_stmt_reset_pw = nullptr;

	sqlite3_stmt *m_stmt_f2b_add = nullptr;
	sqlite3_stmt *m_stmt_f2b_read = nullptr;
	sqlite3_stmt *m_stmt_f2b_cleanup = nullptr;

	sqlite3_stmt *m_stmt_log = nullptr;
};
