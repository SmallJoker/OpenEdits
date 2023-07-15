#pragma once

#include <map>
#include <string>

class Player;

#define CHATCMD_FUNC(name) \
	void (name)(Player *player, std::string msg)

class ChatCommandHandler {};

typedef CHATCMD_FUNC(ChatCommandHandler::*ChatCommandAction);


class ChatCommand {
public:
	ChatCommand(ChatCommandHandler *env);

	void setMain(ChatCommandAction action) { m_action = action; }

	void         add(const std::string &subcmd, ChatCommandAction action);
	ChatCommand &add(const std::string &subcmd);
	const ChatCommand *get(const std::string &subcmd) const;

	bool run(Player *player, std::string msg) const;

	std::string dumpUI() const;

private:
	ChatCommandAction m_action = nullptr;
	ChatCommandHandler *m_env;
	ChatCommand *m_root = nullptr;

	std::map<std::string, ChatCommand> m_subcommands;

};
