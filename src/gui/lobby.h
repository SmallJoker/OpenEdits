#pragma once

#include "gui.h"
#include <string>

class SceneLobby : public SceneHandler {
public:
	SceneLobby();

	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	core::stringw worldid = L"";

private:
};
