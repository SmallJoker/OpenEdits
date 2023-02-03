#include "lobby.h"

SceneLobby::SceneLobby()
{
}

void SceneLobby::draw()
{
	m_gui->joinWorld(this);
}

void SceneLobby::step(float dtime)
{
}

bool SceneLobby::OnEvent(const SEvent &e)
{
	return false;
}

bool SceneLobby::OnEvent(GameEvent &e)
{
	return false;
}
