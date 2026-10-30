#pragma once

#include <string>
#include <vector>

struct ClientStartData;

class LoginManager {
public:
	LoginManager();
	~LoginManager();

	/// Clears all entries and requires `load()` to retrieve them again.
	void clear();

	void load();
	void save() const;

	bool isLoaded() const { return m_loaded; }

	const auto &getEntries() { return m_entries; }
	ClientStartData *find(const std::string &address, const std::string &name);
	void recordLogin(ClientStartData &&data);
	void removeEntry(size_t i);

private:
	bool loadFromTextfile();
	bool loadSerialized();
	void saveSerialized() const;

	std::string m_filename;
	std::vector<ClientStartData> m_entries;
	bool m_loaded = false;
};
