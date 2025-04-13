local world = env.world

local function set_tile_to_1(bx, by)
	local fg, tile, _ = world.get_block(bx, by)
	if tile == 0 then
		world.set_tile(fg, 1, world.PRT_ONE_BLOCK, bx, by)
	end
end

local old_event = env.on_player_event or (function() end)
env.on_player_event = function(event, arg)
	old_event(event, arg)

	if event == "godmode" and env.world.update_tiles then
		env.world.update_tiles({50})
	end
end

local blocks_hidden = {
	{
		id = 50, -- ID_SECRET
		minimap_color = 0x00000001,
		tiles = {
			{ type = env.DRAW_TYPE_SOLID, alpha = true },
			{ type = env.DRAW_TYPE_SOLID }
		},
		on_collide = set_tile_to_1,
		get_visuals = function(tile)
			-- Bad example; should be updated using env.world.set_tile
			return player_data[env.player:hash()].godmode and 1 or 0
		end,
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
			{ type = env.DRAW_TYPE_SOLID },
			{ type = env.DRAW_TYPE_SOLID, alpha = true }
		},
		on_collide = set_tile_to_1,
	},
}

env.register_pack({
	name = "hidden",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_hidden)
})

reg.change_blocks(blocks_hidden)

