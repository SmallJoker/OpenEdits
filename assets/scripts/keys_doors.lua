local world = env.world
local player = env.player

local tiles_door = {
	{ type = env.DRAW_TYPE_SOLID },
	{ type = env.DRAW_TYPE_DECORATION, alpha = true },
}
local tiles_gate = {
	{ type = env.DRAW_TYPE_DECORATION, alpha = true },
	{ type = env.DRAW_TYPE_SOLID },
}

reg.blocks_doors = {
	{ id = 23, tiles = tiles_door }, -- R
	{ id = 24, tiles = tiles_door },
	{ id = 25, tiles = tiles_door },
	{ id = 26, tiles = tiles_gate }, -- R
	{ id = 27, tiles = tiles_gate },
	{ id = 28, tiles = tiles_gate },
}

local keys_blocks_LUT = {}
local KEY_EV
KEY_EV = env.register_event(4, 0, env.PARAMS_TYPE_U8,
	function(key_id)
		print("keys @ " .. (env.server and "server" or "client"), key_id)

		if env.server then
			for _, player in ipairs(env.server.get_players_in_world()) do
				print("Send to " .. player:get_name())
				player:send_event(KEY_EV, key_id)
			end
		else
			local b = keys_blocks_LUT[key_id]
			world.set_tile(b[1], 1, world.PRT_ENTIRE_WORLD)
			world.set_tile(b[2], 1, world.PRT_ENTIRE_WORLD)
		end
	end
)

local function make_key_block(key_id, door_id, gate_id)
	keys_blocks_LUT[key_id] = { door_id, gate_id }
	local def = {
		id = key_id,
		tiles = { { type = env.DRAW_TYPE_DECORATION, alpha = true } },
		on_event = function(payload, bx, by)
		end,
		on_intersect_once = function(_)
			print("send event", KEY_EV)
			env.send_event(KEY_EV, key_id)
		end
	}
	return def
end

reg.blocks_keys = {
	make_key_block(6, 23, 26), -- R
	make_key_block(7, 24, 27), -- G
	make_key_block(8, 25, 28), -- B
}
