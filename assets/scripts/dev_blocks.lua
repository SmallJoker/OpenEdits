local PARTICLE_TEX = "pack_basic.png"
env.require_asset(PARTICLE_TEX)


-- Server --> Client
local EV_ANIMATION
EV_ANIMATION = env.register_event(reg.next_event_id(0) + env.SEF_HAVE_ACTOR, 0, env.PARAMS_TYPE_U8,
	function()
		if not env.have_gui or not gui.spawn_particles then
			return
		end

		env.player:set_smiley({
			texture = "pack_basic.png",
			tile_index = 4,
			size = 1.0,
			visible = false
		})

		gui.spawn_particles(-1, {
			texture = PARTICLE_TEX,
			pos = {
				-- flat array of x, y coordinates
				0, 0,
				0, 0,
				0, 0,
				0, 0,
			},
			time_offsets = { 0.1, 0.8, 2.0, 3.0 },
			age = 0,
			animate = function(self, dtime)
				-- Couldn't this be done in a shader?
				local age = self.age + dtime
				if self.age < 2 and age > 2 then
					env.player:set_smiley({ visible = true })
				end
				self.age = age

				for i, offset in ipairs(self.time_offsets) do
					local t = age + offset
					self.pos[i * 2 - 1] =  1 + 2 * math.cos(t * 0.8)
					self.pos[i * 2 + 0] = -2 + 2 * math.sin(t * 0.8)
				end
				self.size[1] = math.min(2, 8 - age)
				return true
			end,
			tile_index = 3,
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
