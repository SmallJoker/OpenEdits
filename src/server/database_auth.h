#pragma once

#include "database.h"

struct AuthInformation {
	std::string name;
	std::string email;
	std::string password;
	std::string password_reset;

	uint64_t last_login = 0;
	uint64_t resend_retry = 0;
	uint64_t ban_expiry = 0;

	enum AccountLevel : int {
		AL_INVALID = 0,
		AL_PENDING = 1,
		AL_REGISTERED = 2,
		AL_ADMIN = 10
	};
	int level = 0;
};

struct AuthLogEntry {
	uint64_t timestamp = 0;
	std::string action;
	std::string text;
};

class DatabaseAuth : Database {
public:
	DatabaseAuth() : Database() {}
	~DatabaseAuth();

	bool tryOpen(const char *filepath) override;
	void close() override;

	bool load(const std::string &what, AuthInformation *auth);
	bool save(const AuthInformation *auth);

	bool resetPassword(const std::string &email);

	bool logNow(AuthLogEntry entry);

private:
	sqlite3_stmt *m_stmt_read = nullptr;
	sqlite3_stmt *m_stmt_write = nullptr;

	sqlite3_stmt *m_stmt_reset_pw = nullptr;
	sqlite3_stmt *m_stmt_log = nullptr;
};
