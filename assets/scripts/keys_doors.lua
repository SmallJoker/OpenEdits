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

local keys_blocks_LUT = {
	-- [key_block_id] = { door_block_id, gate_block_id }
}

local last_time = 0
local cooldowns = {
	-- [key_block_id] = { [world_id] = turn_off_time, [world_id] = ... }
}

local EV_KEY_ON, EV_KEY_OFF
local function event_set_key(key_id, event_id)
	local is_on = (event_id == EV_KEY_ON)
	print(("@%s | key %d = %s"):format(
		(env.server and "server" or "client"),
		key_id,
		is_on and "ON" or "OFF"
	))

	if env.server then
		for _, player in ipairs(env.server.get_players_in_world()) do
			print("Send to " .. player:get_name())
			player:send_event(event_id, key_id)
		end
		if is_on then
			-- Turn off queue
			cooldowns[key_id][world.get_id()] = last_time + 3
		end
	end

	local b = keys_blocks_LUT[key_id]
	local tile = is_on and 1 or 0
	world.set_tile(b[1], tile, world.PRT_ENTIRE_WORLD)
	world.set_tile(b[2], tile, world.PRT_ENTIRE_WORLD)
end

EV_KEY_ON = env.register_event(4, 0, env.PARAMS_TYPE_U8,
	function(key_id)
		event_set_key(key_id, EV_KEY_ON)
	end
)

EV_KEY_OFF = env.register_event(5, 0, env.PARAMS_TYPE_U8,
	function(key_id)
		event_set_key(key_id, EV_KEY_OFF)
	end
)

-- Delayed turn off event
if env.server then
	local function turn_off_key(world_id, key_id)
		if world.select(world_id) then
			event_set_key(key_id, EV_KEY_OFF)
		end
		cooldowns[key_id][world_id] = nil
	end

	local old_on_step = env.on_step
	env.on_step = function(abstime)
		last_time = abstime
		for key_id, worlds_time in pairs(cooldowns) do
			for world_id, offtime in pairs(worlds_time) do
				if offtime < abstime then
					turn_off_key(world_id, key_id)
				end
			end
		end
	end
end

local function make_key_block(key_id, door_id, gate_id)
	keys_blocks_LUT[key_id] = { door_id, gate_id, nil }
	cooldowns[key_id] = {}
	local def = {
		id = key_id,
		tiles = { { type = env.DRAW_TYPE_DECORATION, alpha = true } },
		on_intersect_once = function(_)
			--print("send event", EV_KEY_ON)
			env.send_event(EV_KEY_ON, key_id)
		end
	}
	return def
end

reg.blocks_keys = {
	make_key_block(6, 23, 26), -- R
	make_key_block(7, 24, 27), -- G
	make_key_block(8, 25, 28), -- B
}
