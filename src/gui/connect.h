#pragma once

#include "render.h"
#include <string>

class SceneHandlerConnect : public SceneHandler {
public:
	SceneHandlerConnect();

	SceneHandlerType runPre(float dtime) override;
	void runPost() override;
	bool OnEvent(const SEvent &e) override;

private:
	int x = -1, y = -1;
	std::string address;
};
