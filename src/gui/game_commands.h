#pragma once

#include "core/chatcommand.h"

class Client;

// The client GUI does not have the flexibility to call the C++ functions
// directly, hence expose some client-side functionality by chat commands.

class GameCommands : public ChatCommandHandler {
public:
	GameCommands();
	void initialize(Client *cli);

	bool process(std::string message);

private:
	CHATCMD_FUNC(chat_Help);
	CHATCMD_FUNC(chat_Export);

	void sendChat(const std::string &message);

	Client *m_client = nullptr;

	ChatCommand m_chatcmd;
};
