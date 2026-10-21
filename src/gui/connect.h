#pragma once

#include "client/client.h"
#include "gui.h"
#include <string>

class SceneConnect : public SceneHandler {
public:
	SceneConnect();

	void OnClose() override;

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	static void recordLogin(ClientStartData data);

private:
	void backupInputs();
	std::string getToken();
	/// elementid == 0 means autodetect connect or hosting
	void onSubmit(int elementid);

	void updatePasswordField(bool is_focused);
	bool m_use_token = false;

	void removeServer(int index);
	void updateServers();

	std::vector<ClientStartData> m_index_to_address;
	/// To restore the input boxes on failure
	/// `.token` is a presence placeholder empty/non-empty
	ClientStartData m_inputs;
};
