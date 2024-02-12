#include "unittest_internal.h"
#include "core/chatcommand.h"
#include "core/environment.h"

static int call_counter = 0;

class TestHandler : public ChatCommandHandler {
public:
	TestHandler() : ChatCommandHandler() {}

	void setupCommands(ChatCommand &cmd, bool assign_main)
	{
		if (assign_main)
			cmd.setMain(CHATCMD_REGISTER(mainCommand));
		cmd.add("!help", CHATCMD_REGISTER(normalCommand));
	}

	CHATCMD_FUNC(normalCommand)
	{
		call_counter += 10;
	}

	CHATCMD_FUNC(mainCommand)
	{
		call_counter--;
	}
};

// NyisBotCPP test copypasta
void unittest_chatcommand()
{
	TestHandler env;
	call_counter = 0;

	{
		ChatCommand cmd;
		env.setupCommands(cmd, true);
		CHECK(cmd.dumpUI().size() > 0);

		// Executes main command -> -1
		CHECK(cmd.run(nullptr, "!foo bar baz") == true);
		CHECK(call_counter == -1);

		// Execute valid "!help" command -> 9
		CHECK(cmd.run(nullptr, "!help") == true);
		CHECK(call_counter == 9);
	}

	{
		// Without main command
		ChatCommand cmd;
		env.setupCommands(cmd, false);
		CHECK(cmd.run(nullptr, "!help") == true);
		CHECK(call_counter == 19);
		// Unhandled case
		CHECK(cmd.run(nullptr, "!invalid") == false);
	}
}
