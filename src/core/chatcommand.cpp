#include "chatcommand.h"
#include "macros.h"
#include "utils.h"

ChatCommand::ChatCommand(Environment *env)
{
	m_env = env;
	if (!m_env)
		m_root = this;
}


void ChatCommand::add(const std::string &subcmd, ChatCommandAction action, Environment *env)
{
	ChatCommand &cmd = add(subcmd, env);
	cmd.setMain(action);
}

ChatCommand &ChatCommand::add(const std::string &subcmd, Environment *env)
{
	ASSERT_FORCED(env || m_env, "ChatCommand instance must be owned by a module");

	auto [it, is_new] = m_subcommands.insert({subcmd, ChatCommand(env ? env : m_env)});
	it->second.m_root = m_root;

	if (!is_new) {
		fprintf(stderr, "Overriding command %s !\n", subcmd.c_str());
		it->second.m_subcommands.clear();
	}

	return it->second;
}

bool ChatCommand::run(Player *player, std::string msg) const
{
	// Main command
	if (m_env && m_action && (msg.empty() || m_subcommands.empty())) {
		(m_env->*m_action)(player, msg);
		return true;
	}

	std::string cmd(get_next_part(msg));

	auto it = m_subcommands.find(cmd);
	if (it != m_subcommands.end()) {
		if (it->second.run(player, msg))
			return true;
	}

	// Show help function if available
	if (m_env && m_action) {
		(m_env->*m_action)(player, msg);
		return true;
	}
	return false;
}

std::string ChatCommand::dumpUI() const
{
	bool first = true;
	std::string str;
	for (const auto &it : m_subcommands) {
		if (!first)
			str.append(", ");

		str.append(it.first);
		if (!it.second.m_subcommands.empty()) {
			// Contains subcommands
			str.append(" [+...]");
		}
		first = false;
	}
	return str;
}


