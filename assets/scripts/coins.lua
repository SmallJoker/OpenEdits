local world = env.world
local player = env.player
local get_pwdata = reg.get_pwdata

env.require_asset("coin.mp3")

local EV_COINS
EV_COINS = env.register_event(100 + env.SEF_HAVE_ACTOR, 0, env.PARAMS_TYPE_U8,
	function(count)
		local pw_data = get_pwdata(player)
		if not env.is_me() then
			-- Client: do not let the server overwrite our data
			pw_data.coins = count
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
		if env.is_me() then
			env.world.update_tiles({43})
		end
		if gui.set_hud then
			pw_data.coins_hud = gui.set_hud(pw_data.coins_hud, {
				type = gui.ELMT_TABLE, grid = { 2, 2 }, fields = {
					[2] = {
						type = gui.ELMT_TEXT,
						text = "Coins: " .. count,
						margin = { 1, 0, 0, 1 }, -- top right
						color = 0xFFFFFF00, -- yellow
					},
				},
				values = {},
				on_input = function() end
			})
		end
	end
)
-- TODO: send EV_COINS to newly joined players via attributes
-- TODO: update count on block place/remove

env.on_block_place = function(x, y, id)
	local old_id, old_tile, _ = env.world.get_block(x, y)
	if old_id == 100 and old_tile > 0 then
		-- TODO
		env.world.update_tiles({43})
	end
end

env.on_world_data = function()
	for _, p in ipairs(env.world.get_players()) do
		print("reset for " .. p:get_name())
		get_pwdata(p).coins = 0
	end
	--env.world.get_blocks_in_range({}, {100}, env.world.PRT_ENTIRE_WORLD)
end


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

			local pd = get_pwdata(player)
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
		overlay = {
			type = gui.TOVT_TEXT_BR,
			fg_color = 0xFF000000,
			bg_color = 0xFFEECC00,
		},
		tiles = {
			{
				type = env.DRAW_TYPE_SOLID,
				--params_mask = 0x000000FF, -- "Which unique params are needed?"
			},
			{ type = env.DRAW_TYPE_SOLID, alpha = true }
		},
		get_visuals = function(tile, coins)
			-- Only called when coming into visible range and there is nothing cached
			-- The returned tile can only be changed if they're not "physics dependent"
			local p_coins = get_pwdata(player).coins
			if p_coins >= coins then
				return 1
			end
			return 0, coins - p_coins
		end,
		on_collide = function(bx, by, is_x)
			-- Called on every player! Do not check against the local `tile`.
			local coins = world.get_params(bx, by)
			local p_coins = get_pwdata(player).coins

			return (p_coins >= coins
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
