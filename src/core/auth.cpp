#include "auth.h"
extern "C" {
#include "sha3.h"
}

constexpr unsigned int SHA3_VARIANT = 384;

std::string Auth::generateRandom()
{
	std::string random(40, '\0');
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

void Auth::hash(const std::string &inp1, const std::string &inp2)
{
	std::string to_hash(inp1 + inp2);
	if (to_hash.size() != inp1.size() + inp2.size()) {
		// Naive check whether \0 did not terminate "data" too early
		fprintf(stderr, "String concat did not work!\n");
		exit(1);
	}

	// Overwrite output data
	output.resize(SHA3_VARIANT / 8);

	sha3_HashBuffer(SHA3_VARIANT, SHA3_FLAGS_KECCAK,
		to_hash.c_str(), to_hash.size(),
		(void *)output.c_str(), output.size()
	);
}
