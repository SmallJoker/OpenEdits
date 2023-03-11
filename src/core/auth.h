#pragma once

#include <string>

class Auth {
public:
	void clear()
	{
		m_pw_hash.clear();
		m_combined_hash.clear();
	}

	// To hash a password from the GUI
	void fromPass(const std::string &password);

	// To load from the database
	void fromHash(const std::string &hash);

	inline const std::string &getPwHash() { return m_pw_hash; }
	inline const std::string &getCombinedHash() { return m_combined_hash; }

	static std::string generateRandom();
	static std::string generatePass();
	void combine(const std::string &random);
	bool verify(const std::string &combined_hash);

	enum class Status {
		Unauthenticated, // needs to login
		Guest,
		Unregistered,
		SignedIn,
	};
	Status status = Status::Unauthenticated;

	std::string random; // for server use

private:
	std::string m_pw_hash;
	std::string m_combined_hash;
};

