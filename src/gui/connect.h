#pragma once

#include "client/client.h"
#include "client/loginmanager.h"
#include "gui.h"
#include <string>

class SceneConnect : public SceneHandler {
public:
	SceneConnect();

	void OnOpen() override;
	void OnClose() override;

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	void recordLogin(ClientStartData data);

private:
	void backupInputs();
	/// elementid == 0 means autodetect connect or hosting
	void onSubmit(int elementid);

	void updatePasswordField(bool is_focused);
	bool m_use_token = false;

	void removeServer(int index);
	void updateServers();

	LoginManager m_loginmgr;
	/// To restore the input boxes on failure
	/// `.token` is a presence placeholder empty/non-empty
	ClientStartData m_inputs;
};
