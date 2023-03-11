#include "auth.h"
extern "C" {
#include "sha3.h"
}

constexpr unsigned int SHA3_VARIANT = 384;

void Auth::fromPass(const std::string &password)
{
	m_pw_hash.resize(SHA3_VARIANT / 8);

	sha3_HashBuffer(SHA3_VARIANT, SHA3_FLAGS_KECCAK,
		password.c_str(), password.size(),
		(void *)m_pw_hash.c_str(), m_pw_hash.size()
	);
}

void Auth::fromHash(const std::string &hash)
{
	m_pw_hash = hash;
}

std::string Auth::generateRandom()
{
	std::string random(20, '\0');
		// Create new random (for server use)
	for (char &c : random)
		c = rand();

	return random;
}

std::string Auth::generatePass()
{
	static const char PASS_CHARACTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrtstuvwxyz0123456789_+&#!";
	std::string random(rand() % 5 + 15, '\0');

	for (char &c : random)
		c = PASS_CHARACTERS[rand() % sizeof(PASS_CHARACTERS)];
	return random;
}

void Auth::combine(const std::string &random)
{
	std::string to_hash(m_pw_hash + random);
	if (to_hash.size() != m_pw_hash.size() + random.size()) {
		// Naive check whether \0 did not terminate "random" too early
		fprintf(stderr, "String concat did not work!\n");
		exit(1);
	}

	m_combined_hash.resize(SHA3_VARIANT / 8);

	sha3_HashBuffer(SHA3_VARIANT, SHA3_FLAGS_KECCAK,
		to_hash.c_str(), to_hash.size(),
		(void *)m_combined_hash.c_str(), m_combined_hash.size()
	);
}

bool Auth::verify(const std::string &combined_hash)
{
	if (m_combined_hash.empty()) {
		fprintf(stderr, "Mixed hash is not initialized!\n");
		return false;
	}

	return m_combined_hash == combined_hash;
}
