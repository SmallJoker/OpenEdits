local world = env.world
local player = env.player

env.require_asset("coin.mp3")

local EV_COINS
EV_COINS = env.register_event(100 + env.SEF_HAVE_ACTOR, 0, env.PARAMS_TYPE_U8,
	function(count)
		if not env.is_me() then
			-- Client: do not let the server overwrite our data
			player_data[env.player:hash()].coins = count
		end
		print(("@%s | coins of %s: %d"):format(
			(env.server and "server" or "client"),
			player:get_name(),
			count
		))

		if env.server then
			-- Let the other players know
			env.send_event(EV_COINS, count)
		end
	end
)
-- TODO: send EV_COINS to newly joined players via attributes
-- TODO: update count on block place/remove

local blocks_coins = {
	{
		id = 100,
		tiles = { { alpha = true }, { alpha = true } },
		on_intersect_once = function(tile)
			if tile ~= 0 or not env.is_me() then
				return -- cannot collect (again)
			end

			local px, py = player:get_pos()
			world.set_tile(100, 1, world.PRT_ONE_BLOCK, px, py)

			local pd = player_data[player:hash()]
			pd.coins = pd.coins + 1
			env.send_event(EV_COINS, pd.coins)

			if gui then
				gui.play_sound("coin.mp3")
			end
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
			end,
			on_place = function(values, x, y)
				-- must match the "params" type
				gui.select_block(nil, values.coins)
			end,
		},
		params = env.PARAMS_TYPE_U8,
		tiles = {
			{ type = env.DRAW_TYPE_SOLID },
			{ type = env.DRAW_TYPE_SOLID, alpha = true }
		},
		on_collide = function(bx, by, is_x)
			-- Called on every player! Do not check against the local `tile`.
			local coins = world.get_params(bx, by)
			local pd = player_data[player:hash()]

			return (pd.coins >= coins
				and env.COLLISION_TYPE_NONE
				or env.COLLISION_TYPE_POSITION)
		end,
	}
}

env.register_pack({
	name = "coins",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_coins)
})

reg.change_blocks(blocks_coins)
