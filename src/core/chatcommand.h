#pragma once

#include <map>
#include <string>

class Player;
class Environment;

#define CHATCMD_FUNC(name) \
	void (name)(Player *player, std::string msg)

typedef CHATCMD_FUNC(Environment::*ChatCommandAction);


class ChatCommand {
public:
	ChatCommand(Environment *env);

	void setMain(ChatCommandAction action) { m_action = action; }

	void         add(const std::string &subcmd, ChatCommandAction action, Environment *env = nullptr);
	ChatCommand &add(const std::string &subcmd, Environment *env = nullptr);
	const ChatCommand *get(const std::string &subcmd) const;

	bool run(Player *player, std::string msg) const;

	std::string dumpUI() const;

private:
	ChatCommandAction m_action = nullptr;
	Environment *m_env;
	ChatCommand *m_root = nullptr;

	std::map<std::string, ChatCommand> m_subcommands;

};
