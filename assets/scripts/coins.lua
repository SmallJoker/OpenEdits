local table_to_pack_blocks = reg.table_to_pack_blocks
local change_blocks = reg.change_blocks
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
				if env.gui then
					env.gui.play_sound("coin.mp3")
				end
			end
			-- env.world.event(42, can_predict)
		end
	},
	{
		id = 43,
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
	blocks = table_to_pack_blocks(blocks_coins)
})

change_blocks(blocks_coins)
