#pragma once

#include "gui.h"
#include <string>

class SceneConnect : public SceneHandler {
public:
	SceneConnect();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	core::stringw nickname = L"Guest420";
	core::stringw password;
	core::stringw address = L"127.0.0.1";
	bool start_localhost = false;

private:
	void onSubmit(int elementid);

	struct LoginInfo {
		std::wstring nickname;
		std::wstring address;
	};
	std::vector<LoginInfo> m_index_to_address;
};
