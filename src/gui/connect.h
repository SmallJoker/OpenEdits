#pragma once

#include "gui.h"
#include <string>

class SceneConnect : public SceneHandler {
public:
	SceneConnect();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(const GameEvent &e) override;

	core::stringw nickname = L"Guest420";
	core::stringw address = L"127.0.0.1";
	bool start_localhost = false;

private:
	void onSubmit(int elementid);

	int x = -1, y = -1;
};
