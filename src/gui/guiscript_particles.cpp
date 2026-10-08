#include "guiscript.h"
#include "guiscript_internal.h"

#include "client/client.h"
#include "client/clientmedia.h"
#include "client/localplayer.h"
#include "core/logger.h"
#include "core/profiler.h"
#include "core/script/playerref.h"
#include "core/script/script_utils.h"
#include "particles.h"
// Irrlicht-related
#include "CParticlesSceneNode.h"
#include <IGUIEnvironment.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>

using namespace ScriptUtils;

extern Logger guiscript_logger;
static Logger &logger = guiscript_logger;

Particles::Particles() :
	bbox({0,0,0})
{
}

Particles::~Particles()
{
	if (ref_table != LUA_NOREF)
		abort();
}

void Particles::remove(lua_State *L)
{
	if (scene_node.get())
		scene_node->remove();

	luaL_unref(L, LUA_REGISTRYINDEX, ref_table);
	ref_table = LUA_NOREF;
}



// -------------- Particles -------------

void GuiScript::updateParticles(float dtime)
{
	logger(LL_DEBUG, "%s %g", __func__, dtime);
	std::vector<int> to_remove;

	for (auto &it : m_particles) {
		Particles &p = it.second;
		p.expiry -= dtime;

		bool remove =
			p.expiry <= 0.0f
			// Player left the world
			|| (p.relative_to && !m_client->getPlayerNoLock(p.relative_to));

		if (remove) {
			p.remove(m_lua);
			to_remove.emplace_back(it.first);
			continue;
		}

		// TODO: Run at 1/10th the interval if not visible
		bool is_visible = true;
		float interval = std::max(p.interval, 0.01f);
		if (!is_visible)
			interval = std::min(interval * 2.0f, 0.5f);

		const float p_dtime = (p.interval_elapsed += dtime);
		if (p_dtime < interval)
			continue;
		p.interval_elapsed = 0;

		animateParticle(p, p_dtime);
	}

	for (int id : to_remove)
		m_particles.erase(id);
}

void GuiScript::removeParticles()
{
	for (auto &it : m_particles)
		it.second.remove(m_lua);
	m_particles.clear();
}

void GuiScript::animateParticle(Particles &p, float dtime)
{
	static Profiler profiler(__func__);
	ScopeProfiler sp(profiler);

	// Use a protected call to execute 'animate' and reading the table.
	lua_State *L = m_lua;

	int top = lua_gettop(L);
	lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
	int errorhandler = lua_gettop(L);

	// Function
	lua_pushcfunction(L, read_particles);
	// Arguments
	lua_pushlightuserdata(L, &p);
	lua_pushnumber(L, dtime);

	int status = lua_pcall(L, 2, 0, errorhandler);
	if (status != 0) {
		const char *err = lua_tostring(L, -1);
		logger(LL_ERROR, "%s failed: %s. Definition: %s", __func__, err, p.backtrace.c_str());
		p.expiry = 0.0f; // avoid spam. mark for removal.
	} else {
		if (lua_toboolean(L, -1)) {
			// Queue for update
			if (p.scene_node)
				p.scene_node->remove();
			p.scene_node.reset();
		}
	}

	lua_settop(L, top);
}

int GuiScript::read_particles(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START
	Particles &p = *(Particles *)lua_topointer(L, 1);
	bool changed = false;

	lua_rawgeti(L, LUA_REGISTRYINDEX, p.ref_table);
	lua_getfield(L, -1, "animate");
	if (!lua_isnil(L, -1)) {
		lua_pushvalue(L, -2); // Particle Definition table (ref_table)
		lua_pushvalue(L, 2);  // dtime
		lua_call(L, 2, 1);

		if (lua_isboolean(L, -1)) {
			if (lua_toboolean(L, -1))
				changed = true;
			else
				p.expiry = 0; // remove
		}
		lua_pop(L, 1); // return value
	} else {
		lua_pop(L, 1); // field
	}

	if (!changed) {
		lua_pushboolean(L, false);
		return 1;
	}

#define M FLT_MAX
	core::aabbox3d<f32> bbox({{M,M,M}, {-M,-M,-M}});
#undef M

	// Stack: [-3] = Particles*, [-2] = dtime, [-1] = table
	get_check_field(L, 3, "pos", LUA_TTABLE);
	p.pos.resize(lua_objlen(L, -1) / 2);
	for (unsigned i = 0; i < p.pos.size(); ++i) {
		lua_rawgeti(L, -1, i * 2 + 1); // x @ -2
		lua_rawgeti(L, -2, i * 2 + 2); // y @ -1

		core::vector2df pos(
			lua_tonumber(L, -2),
			lua_tonumber(L, -1)
		);
		p.pos[i] = pos;

		lua_pop(L, 2);

		// OpenGL Y goes up, world Y goes down.
		bbox.addInternalPoint(pos.X, -pos.Y, 0.0f);
	}
	lua_pop(L, 1); // pos

	// Extra margin to take size into account (max 4, any rotation)
	bbox.MaxEdge += 4 * M_SQRT2;
	bbox.MinEdge -= 4 * M_SQRT2;
	p.bbox = bbox;

	get_check_field(L, 3, "size", LUA_TTABLE);
	p.size.resize(lua_objlen(L, -1));
	for (unsigned i = 0; i < p.size.size(); ++i) {
		lua_rawgeti(L, -1, i + 1);

		p.size[i] = lua_tonumber(L, -1);

		lua_pop(L, 1);
	}
	lua_pop(L, 1); // size


	/* TODO:
		rotation
		frame_index
		z_index
	*/

	lua_pushboolean(L, true);
	return 1;
	MESSY_CPP_EXCEPTIONS_END
}


int GuiScript::l_gui_spawn_particles(lua_State *L)
{
	MESSY_CPP_EXCEPTIONS_START

	int id = -1;
	if (!lua_isnil(L, 1))
		id = luaL_checkinteger(L, 1);

	luaL_checktype(L, 2, LUA_TTABLE);

	GuiScript *script = static_cast<GuiScript *>(get_script(L));
	Particles &p = get_free_slot(L, "Particles",
			script->m_particles, script->m_particles_id_next, id);
	p.remove(L);

	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, CUSTOM_RIDX_TRACEBACK);
		lua_call(L, 0, 1);
		p.backtrace = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	{
		lua_pushvalue(L, 2);
		p.ref_table = luaL_ref(L, LUA_REGISTRYINDEX);
		if (p.ref_table < 0)
			luaL_error(L, "Failed to reference Particles table id=%d\n", id);
	}

	// Values used by 'read_particles'
	{
		(void)get_check_field_or_nil(L, 2, "animate", LUA_TFUNCTION);
		(void)get_check_field_or_nil(L, 2, "pos",  LUA_TTABLE);
		(void)get_check_field_or_nil(L, 2, "size", LUA_TTABLE);
		lua_pop(L, 3);
	}

	const char *texture = check_field_string(L, 2, "texture");
	p.texture_path = script->m_client->getMedia()->getAssetPath(texture);
	if (!p.texture_path)
		luaL_error(L, "unknown texture");

	get_check_field(L, 2, "expiry", LUA_TNUMBER);
	const float expiry = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (get_check_field_or_nil(L, 2, "interval", LUA_TNUMBER))
		p.interval = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (get_check_field_or_nil(L, 2, "tile_index", LUA_TNUMBER))
		p.tile_index = lua_tointeger(L, 2);
	lua_pop(L, 1);

	if (get_check_field_or_nil(L, 2, "relative_to", LUA_TUSERDATA)) {
		Player *player = PlayerRef::toPlayerRef(L, -1)->ptrRef();
		if (!player)
			luaL_error(L, "unknown player");

		p.relative_to = player->peer_id;
	}
	lua_pop(L, 1);

	// final touch after success: make it not expire.
	p.expiry = expiry;
	return 0;
	MESSY_CPP_EXCEPTIONS_END
}
