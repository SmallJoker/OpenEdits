#pragma once

#include "render.h"
#include <string>

class SceneHandlerConnect : public SceneHandler {
public:
	SceneHandlerConnect();

	void draw() override;
	SceneHandlerType step(float dtime) override;
	bool OnEvent(const SEvent &e) override;

private:
	int x = -1, y = -1;
	std::string address;
};
