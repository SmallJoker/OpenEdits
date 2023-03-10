#pragma once

#include <string>

class Auth {
public:
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
	bool mayLogin() { return is_guest || m_is_authenticated; }

	bool is_guest = false;

private:
	std::string m_pw_hash;
	std::string m_combined_hash;

	bool m_is_authenticated = false;
};

