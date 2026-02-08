local world = env.world

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

local key_data = {}
local last_time = 0

local function event_set_key(key_id, event_id)
	local kdata = key_data[key_id]
	local is_on = event_id == kdata.ev_on

	if env.server and is_on then
		local skip = kdata.cooldowns[world.get_id()]
		-- Refresh cooldown for turning off
		kdata.cooldowns[world.get_id()] = last_time + 3

		if skip then
			-- Already active. Do not resend.
			return
		end
	end

	print(("@%s | key %d = %s"):format(
		(env.server and "server" or "client"),
		key_id,
		is_on and "ON" or "OFF"
	))

	if env.server then
		env.send_event(event_id)
	end

	local tile = is_on and 1 or 0
	world.set_tile(kdata.door_id, tile, world.PRT_ENTIRE_WORLD)
	world.set_tile(kdata.gate_id, tile, world.PRT_ENTIRE_WORLD)
end

-- Delayed turn off event
if env.server then
	local function turn_off_key(kdata, world_id, key_id)
		kdata.cooldowns[world_id] = nil
		if world.select(world_id) then
			event_set_key(key_id, kdata.ev_off)
		end
	end

	local old_on_step = env.on_step or (function() end)
	env.on_step = function(abstime)
		old_on_step(abstime)

		last_time = abstime
		for key_id, kdata in pairs(key_data) do
			for world_id, offtime in pairs(kdata.cooldowns) do
				if offtime < abstime then
					turn_off_key(kdata, world_id, key_id)
				end
			end
		end
	end
end

local function make_key_block(key_id, door_id, gate_id)
	key_data[key_id] = {
		door_id = door_id,
		gate_id = gate_id,
		cooldowns = {}
	}
	local kdata = key_data[key_id]

	kdata.ev_on = env.register_event(reg.next_event_id(0), 0,
		function()
			event_set_key(key_id, kdata.ev_on)
		end
	)

	kdata.ev_off = env.register_event(reg.next_event_id(0), 0,
		function()
			event_set_key(key_id, kdata.ev_off)
		end
	)

	local def = {
		id = key_id,
		tiles = { { type = env.DRAW_TYPE_DECORATION, alpha = true } },
		on_intersect_once = function(_)
			--print("send event", EV_KEY_ON)
			env.send_event(kdata.ev_on)
		end
	}
	return def
end

reg.blocks_keys = {
	make_key_block(6, 23, 26), -- R
	make_key_block(7, 24, 27), -- G
	make_key_block(8, 25, 28), -- B
}
