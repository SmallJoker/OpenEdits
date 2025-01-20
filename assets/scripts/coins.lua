local world = env.world

env.require_asset("coin.mp3")

local anyones_coins = 0
local blocks_coins = {
	{
		id = 100,
		tiles = { { alpha = true }, { alpha = true } },
		on_intersect_once = function(tile)
			if tile == 0 then
				local px, py = env.player:get_pos()
				world.set_tile(100, 1, world.PRT_ONE_BLOCK, px, py)
				anyones_coins = anyones_coins + 1
				if gui then
					gui.play_sound("coin.mp3")
				end
			end
			-- env.world.event(42, can_predict)
		end
	},
	{
		id = 43, -- Coindoor
		gui_def = {
			-- root element
			type = gui.ELMT_TABLE, grid = { 2, 1 }, fields = {
				{ type = gui.ELMT_TEXT, text = "coins" },
				{ type = gui.ELMT_INPUT, name = "coins" },
			},
			values = { ["coins"] = 10 },
			on_input = function(values, k, v)
				if k == "coins" then
					v = tonumber(v) and v or values[k]
				end
				values[k] = v
				-- must match the "params" type
				gui.select_params(values.coins)
			end,
		},
		params = env.PARAMS_TYPE_U8,
		tiles = { { type = env.DRAW_TYPE_SOLID }, { type = env.DRAW_TYPE_SOLID, alpha = true } },
		on_collide = function(bx, by, is_x)
			local coins = world.get_params(bx, by)

			return (anyones_coins >= coins
				and env.COLLISION_TYPE_NONE
				or env.COLLISION_TYPE_POSITION)
		end
	}
}

env.register_pack({
	name = "coins",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_coins)
})

reg.change_blocks(blocks_coins)
