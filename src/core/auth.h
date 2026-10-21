#pragma once

#include <string>

/// NOTE: Sent over the network
enum class LoginMethod {
	PasswordNoSave = 0,
	PasswordNewToken,
	Token,
	INVALID
};

class Auth {
public:
	static constexpr size_t CHALLENGE_LENGTH = 40;
	static std::string generate_challenge();
	static std::string generatePass();

	static std::string time_to_string(time_t server_time);
	/// Similar to Packet::dump()
	static void dump_bytes(const char *header, const std::string &data);

	void hash(const std::string &inp1, const std::string &inp2);
	void rehash(const std::string &inp2) { hash(output, inp2); }

	enum class Status {
		Unauthenticated, // needs to login
		Guest,
		Unregistered,
		SignedIn,
	};
	Status status = Status::Unauthenticated;

	std::string salt_1_const; // database-specific
	std::string salt_challenge; // per login/pw change attempt

	std::string output;
};

