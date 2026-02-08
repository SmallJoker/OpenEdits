local world = env.world

local function set_tile_to_1(fg)
	return function(bx, by)
		world.set_tile(fg, 1, world.PRT_ONE_BLOCK, bx, by)
	end
end

local old_event = env.on_player_event or (function() end)
env.on_player_event = function(event, arg)
	old_event(event, arg)

	if event == "godmode" then
		env.world.set_tile(50,  arg and 1 or -1, world.PRT_ENTIRE_WORLD + world.PROP_ADD)
		env.world.set_tile(243, arg and 1 or -1, world.PRT_ENTIRE_WORLD + world.PROP_ADD)
	end
end

local blocks_hidden = {
	{
		id = 50, -- ID_SECRET
		minimap_color = 0x00000001,
		tiles = {
			{ type = env.DRAW_TYPE_SOLID, alpha = true, override = { id = 0, tile = 0} },
			{ type = env.DRAW_TYPE_SOLID }
		},
		on_collide = set_tile_to_1(50),
	},
	{
		id = 44, -- ID_BLACKREAL
		minimap_color = 0xFF000000,
		tiles = {
			{ type = env.DRAW_TYPE_SOLID },
		}
	},
	{
		id = 243, -- ID_BLACKFAKE
		minimap_color = 0xFF000000,
		tiles = {
			{ type = env.DRAW_TYPE_ACTION, override = { id = 44, tile = 0} },
			{ type = env.DRAW_TYPE_ACTION }
		},
		on_collide = set_tile_to_1(243),
	},
}

env.register_pack({
	name = "hidden",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_hidden)
})

reg.change_blocks(blocks_hidden)

