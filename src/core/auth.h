#pragma once

#include <string>

class Auth {
public:
	static std::string generateRandom();
	static std::string generatePass();

	void hash(const std::string &inp1, const std::string &inp2);
	void rehash(const std::string &inp2) { hash(output, inp2); }
	bool rehashByTime(time_t t_server, time_t t_client, bool check_t_server);

	enum class Status {
		Unauthenticated, // needs to login
		Guest,
		Unregistered,
		SignedIn,
	};
	Status status = Status::Unauthenticated;

	time_t salt_time = 0; // used temporarily
	std::string salt_1_const; // database-specific
	std::string salt_challenge; // per login/pw change attempt

	std::string output;
};

