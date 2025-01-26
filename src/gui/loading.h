// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "gui.h"
#include "core/friends.h"
#include "core/timer.h"
#include <string>

namespace irr {
	namespace gui {
		class IGUIButton;
		class IGUIEditBox;
		class IGUIListBox;
		class IGUITabControl;
	}
}

class SceneLoading : public SceneHandler {
public:
	SceneLoading();

	void OnOpen() override;
	void draw() override;
	void step(float dtime) override;
	bool OnEvent(const SEvent &e) override;
	bool OnEvent(GameEvent &e) override;

	enum class Type {
		Invalid,
		ConnectServer,
		JoinWorld
	} loading_type = Type::Invalid;

private:
	void cancel();

	Timer m_timer;
};
