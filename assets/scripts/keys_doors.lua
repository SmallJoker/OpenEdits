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

local is_key_active = {}

local function make_key_block(id, door_id, gate_id)
	local def = {
		id = id,
		tiles = { { type = env.DRAW_TYPE_DECORATION, alpha = true } },
		on_event = function(payload, bx, by)
			world.set_tile(door_id, 1, world.PRT_ENTIRE_WORLD)
			world.set_tile(gate_id, 1, world.PRT_ENTIRE_WORLD)
		end,
		on_intersect_once = function(_)
			world.event(id, 0)
		end
	}
	return def
end

reg.blocks_keys = {
	make_key_block(6, 23, 26),
	make_key_block(7, 24, 27),
	make_key_block(8, 25, 28),
}
