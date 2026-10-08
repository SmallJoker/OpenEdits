#pragma once

#include "core/macros.h" // peer_t
#include <aabbox3d.h>
#include <irrPtr.h>
#include <vector2d.h>
#include <string>
#include <vector>

struct lua_State;
class CParticlesSceneNode;

using namespace irr;

namespace irr::video {
	class ITexture;
}

struct Particles {
	// Implementation in guiscript.cpp
	Particles();
	~Particles();
	void remove(lua_State *L);

	float expiry = -1.0f;
	float interval = 0;
	float interval_elapsed = 0;
	int ref_table = -2; // LUA_NOREF
	std::string backtrace; ///< information to throw later

	peer_t relative_to = 0;

	std::vector<core::vector2df> pos;
	std::vector<float> size;
	const char *texture_path = nullptr;
	u8 tile_index = 0;
	core::aabbox3df bbox;
	irr_ptr<CParticlesSceneNode> scene_node;
};
