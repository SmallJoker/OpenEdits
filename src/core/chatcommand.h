#pragma once

#include <map>
#include <string>
// I wish MSVC would always use long pointers but NO !?!
// warning C4407 indeed results in nonfunctional code regardles
// whether or not /vmv is specified.
#include <functional>

class Player;

#define CHATCMD_FUNC(name) \
	void (name)(Player *player, std::string msg)

class ChatCommandHandler {};

typedef std::function<void(Player *, std::string)> ChatCommandAction;
#define CHATCMD_REGISTER(name) \
	[this] (Player *player, std::string msg) { (name)(player, msg); }

class ChatCommand {
public:
	// Pray to god that you only pass arguments of type CHATCMD_FUNC here!
	void setMain(ChatCommandAction action) { m_action = action; };

	void         add(const std::string &subcmd, ChatCommandAction action);
	ChatCommand &add(const std::string &subcmd);
	const ChatCommand *get(const std::string &subcmd) const;

	bool run(Player *player, std::string msg) const;

	std::string dumpUI() const;

private:
	ChatCommandAction m_action = nullptr;

	std::map<std::string, ChatCommand> m_subcommands;
};
