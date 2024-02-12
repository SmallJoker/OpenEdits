#include "chatcommand.h"
#include "macros.h"
#include "utils.h"


void ChatCommand::add(const std::string &subcmd, ChatCommandAction action)
{
	ChatCommand &cmd = add(subcmd);
	cmd.setMain(action);
}

ChatCommand &ChatCommand::add(const std::string &subcmd)
{
	auto [it, is_new] = m_subcommands.insert({subcmd, ChatCommand()});

	if (!is_new) {
		fprintf(stderr, "Overriding command %s !\n", subcmd.c_str());
		it->second.m_subcommands.clear();
	}

	return it->second;
}

const ChatCommand *ChatCommand::get(const std::string &subcmd) const
{
	auto it = m_subcommands.find(subcmd);
	if (it == m_subcommands.end())
		return nullptr;

	return &it->second;
}


bool ChatCommand::run(Player *player, std::string msg) const
{
	// Main command
	if (m_action && (msg.empty() || m_subcommands.empty())) {
		m_action(player, msg);
		return true;
	}

	std::string cmd(get_next_part(msg));

	auto it = m_subcommands.find(cmd);
	if (it != m_subcommands.end()) {
		if (it->second.run(player, msg))
			return true;
	}

	// Show help function if available
	if (m_action) {
		m_action(player, msg);
		return true;
	}
	return false;
}

std::string ChatCommand::dumpUI() const
{
	std::string str;
	for (const auto &it : m_subcommands) {
		if (!str.empty())
			str.append(", ");

		str.append(it.first);
		if (!it.second.m_subcommands.empty()) {
			// Contains subcommands
			str.append(" [+...]");
		}
	}
	return str;
}


