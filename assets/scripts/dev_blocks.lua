local PARTICLE_TEX = "missing_texture.png"
env.require_asset(PARTICLE_TEX)


-- Server --> Client
local EV_ANIMATION
EV_ANIMATION = env.register_event(reg.next_event_id(0) + env.SEF_HAVE_ACTOR, 0, env.PARAMS_TYPE_U8,
	function()
		if not env.have_gui or not gui.spawn_particles then
			return
		end

		gui.spawn_particles(-1, {
			texture = PARTICLE_TEX,
			pos = {
				-- flat array of x, y coordinates
				0, 0
			},
			age = 0,
			animate = function(self, dtime)
				-- Couldn't this be done in a shader?
				local age = self.age + dtime
				self.age = age

				self.pos[1] =  1 + 2 * math.cos(age * 0.8)
				self.pos[2] = -2 + 2 * math.sin(age * 0.8)
				self.size[1] = math.min(2, 8 - age)
				return true
			end,
			z_index = {-5}, -- TODO
			frame_index = {0},
			size = {0},
			interval = 0,
			relative_to = env.player,
			expiry = 10, -- seconds
		})
	end
)

local old_event = env.on_player_event
env.on_player_event = function(event, arg)
	old_event(event, arg)

	if event == "join" and env.server then
		env.player:send_event(EV_ANIMATION, 0)
	end
end
