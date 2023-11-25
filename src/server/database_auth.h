#pragma once

#include "database.h"
#include <ctime>
#include <vector> // friends

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

struct AuthFriend {
	// sorted alphabetically for the database
	struct Entry {
		std::string name;
		int status = 0;
	} p1, p2;

	// void *metadata;
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
	DatabaseAuth();
	~DatabaseAuth();

	bool tryOpen(const char *filepath) override;
	void close() override;

	// Maybe change this to a Settings object?
	bool getConfig(AuthConfig *entry);
	bool setConfig(const AuthConfig &entry);

	/// Returns false on error or when `auth`==nullptr
	bool load(const std::string &name, AuthAccount *auth);
	bool save(const AuthAccount &auth);

	const std::string &getUniqueSalt();
	bool setPassword(const std::string &name, const std::string &hash);

	/// AuthFriend::p1 is guaranteed to correspond to "name"
	/// Returns false on error or when `friends`==nullptr
	bool listFriends(const std::string &name, std::vector<AuthFriend> *friends);
	/// Inserts or overwrites an existing friend record
	bool setFriend(AuthFriend f);
	bool removeFriend(const std::string &name1, const std::string &name2);

	bool ban(const AuthBanEntry &entry);
	// returns whether an active ban was found
	bool getBanRecord(const std::string &affected, const std::string &context, AuthBanEntry *entry);
	bool cleanupBans();

	bool logNow(AuthLogEntry entry);


private:
	enum {
		STMT_CFG_READ,
		STMT_CFG_WRITE,
		STMT_CFG_REMOVE,

		STMT_AUTH_READ,
		STMT_AUTH_WRITE,
		STMT_AUTH_SET_PW,

		STMT_FRIENDS_LIST,
		STMT_FRIENDS_WRITE,
		STMT_FRIENDS_REMOVE,

		STMT_F2B_INSERT,
		STMT_F2B_READ,
		STMT_F2B_CLEANUP,

		STMT_LOG_INSERT,

		STMT_MAX
	};

	sqlite3_stmt *m_stmt[STMT_MAX];

	std::string m_unique_salt;
};
