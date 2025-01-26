#include "gui/game_commands.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "core/eeo_converter.h"
#include "core/packet.h"
#include "core/player.h"
#include "core/utils.h"
#include "core/worldmeta.h"
#include <fstream>

GameCommands::GameCommands()
{
}

void GameCommands::initialize(Client *cli)
{
	bool has_run_once = m_client;
	m_client = cli;

	if (has_run_once)
		return;

	// Chat command registration
	m_chatcmd.add(".help", CHATCMD_REGISTER(chat_Help));
	m_chatcmd.add(".export", CHATCMD_REGISTER(chat_Export));
}

bool GameCommands::process(std::string message)
{
	message = strtrim(message);
	if (message.empty() || message[0] != '.')
		return false;

	try {
		auto player = m_client->getMyPlayer();
		m_chatcmd.run(player.ptr(), message);
	} catch (std::runtime_error &e) {
		sendChat(std::string("Error while executing command: ") + e.what());
	}

	return true;
}

void GameCommands::sendChat(const std::string &message)
{
	GameEvent e(GameEvent::C2G_LOCAL_CHAT);
	e.player_chat = new GameEvent::PlayerChat();
	e.player_chat->message = message;
	m_client->sendNewEvent(e);
}


// -------------- Chat commands -------------

CHATCMD_FUNC(GameCommands::chat_Help)
{
	std::string cmd(get_next_part(msg));
	if (cmd.empty()) {
		sendChat("Use '.help CMD' for details. Available commands: " + m_chatcmd.dumpUI());
		return;
	}

	static const struct {
		const std::string cmd;
		const std::string text;
	} help_LUT[] = {
		{ "export", "Syntax: .export PREFIX" }
	};

	// Trim "." if necessary
	if (cmd[0] == '.')
		cmd = cmd.substr(1);

	for (auto v : help_LUT) {
		if (cmd != v.cmd)
			continue;

		std::string answer = v.text;

		auto main = m_chatcmd.get("." + v.cmd);
		if (main) {
			std::string subcmds = main->dumpUI();
			if (!subcmds.empty()) {
				answer.append("\nSubcommands: ");
				answer.append(subcmds);
			}
		}
		sendChat(answer);
		return;
	}

	sendChat("No help available for command " + cmd);
}


CHATCMD_FUNC(GameCommands::chat_Export)
{
	std::string filename(get_next_part(msg));
	auto world = player->getWorld();

	filename += "_" + world->getMeta().id + "_" + world->getMeta().owner + ".eelvl";

	{
		std::ifstream is(filename);
		if (is.good()) {
			sendChat("The file \"" + filename + "\" already exists.");
			return;
		}
	}

	EEOconverter conv(*world);
	conv.toFile(filename);

	sendChat("Exported to: " + filename);
}
