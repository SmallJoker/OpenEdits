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

	bool start_localhost = false;

	struct LoginInfo {
		std::wstring nickname,
			password, // not saved.
			address;
	};

	static void recordLogin(ClientStartData data);

private:
	void onSubmit(int elementid);

	void removeServer(int index);
	void updateServers();

	std::vector<LoginInfo> m_index_to_address;
	LoginInfo m_login;
};
