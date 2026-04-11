local world = env.world
local player = env.player
local get_pwdata = reg.get_pwdata
local dprint = false and print or function() end

local ID_COIN = 100
local ID_COINDOOR = 43
local ID_COINGATE = 165

env.require_asset("coin.mp3")

local hud_dirty = false
local function update_hud(pw_data)
	hud_dirty = false
	if not gui.set_hud then
		return
	end

	pw_data = pw_data or reg.get_pwdata(reg.my_player_id)
	if pw_data.coins == 0 then
		if pw_data.coins_hud then
			gui.remove_hud(pw_data.coins_hud)
			pw_data.coins_hud = nil
		end
		return
	end

	local text = "Coins: " .. pw_data.coins
	local _, counts = world.get_blocks_in_range(
		{instances = false, counts = true},
		{ID_COIN}, env.world.PRT_ENTIRE_WORLD
	)
	if counts then
		text = text .. " / " .. counts[ID_COIN]
	end

	pw_data.coins_hud = gui.set_hud(pw_data.coins_hud, {
		type = gui.ELMT_TABLE, grid = { 1, 1 }, fields = {
			[1] = {
				type = gui.ELMT_TEXT,
				text = text,
				margin = { 1, 0, 0, 1 }, -- top right
				color = 0xFFFFFF00, -- yellow
			},
		},
		values = {},
		on_input = function() end
	})
end

local EV_COINS
EV_COINS = env.register_event(ID_COIN + env.SEF_HAVE_ACTOR, 0, env.PARAMS_TYPE_U8,
	function(count)
		local pw_data = get_pwdata(player)
		if not env.is_me() then
			-- Client: do not let the server overwrite our data
			pw_data.coins = count
		end
		dprint(("@%s | coins of %s: %d"):format(
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

-- TODO: send EV_COINS to newly joined players

reg.register_on_block_place({
	check_prev = true,
	fg = ID_COIN,
	action = function(x, y, old_tile)
		local pd = reg.get_pwdata(reg.my_player_id)
		if old_tile > 0 then
			pd.coins = math.max(pd.coins - 1, 0)
			env.world.update_tiles({ID_COINDOOR, ID_COINGATE})
		end

		hud_dirty = true
	end
})

local old_on_step = env.on_step
env.on_step = function(dtime)
	old_on_step(dtime)

	if hud_dirty then
		update_hud()
	end
end

env.on_world_data = function()
	for _, p in ipairs(env.world.get_players()) do
		dprint("reset for " .. p:get_name())
		get_pwdata(p).coins = 0
	end
	update_hud()
end

local function make_coin_block(override)
	local ret = {
		gui_def = {
			-- root element
			type = gui.ELMT_TABLE, grid = { 2, 1 }, fields = {
				{ type = gui.ELMT_TEXT, text = "coins" },
				{ type = gui.ELMT_INPUT, name = "coins" },
			},
			values = { ["coins"] = 10 },
			from_block = function(values, coins)
				values.coins = coins
			end,
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
		get_visuals = function(tile, coins)
			-- Only called when coming into visible range and there is nothing cached
			-- The returned tile can only be changed if they're not "physics dependent"
			local p_coins = get_pwdata(player).coins
			if p_coins >= coins then
				return 1
			end
			return 0, coins - p_coins
		end,
	}
	for k, v in pairs(override) do
		ret[k] = v
	end
	return ret
end

local blocks_coins = {
	{
		id = 100,
		tiles = {
			{ alpha = true, animation_count = 2, animation_delay = 1.2 },
			{ alpha = true }
		},
		on_intersect_once = function(tile)
			if tile ~= 0 or not env.is_me() then
				return -- cannot collect (again)
			end

			local px, py = player:get_pos()
			world.set_tile(100, 1, world.PRT_ONE_BLOCK, px, py)
			world.update_tiles({ID_COINDOOR, ID_COINGATE})

			local pd = get_pwdata(player)
			pd.coins = pd.coins + 1
			env.send_event(EV_COINS, pd.coins)

			if env.have_gui then
				gui.play_sound("coin.mp3")
				hud_dirty = true
			end
		end
	},
	make_coin_block({
		id = ID_COINDOOR,
		tiles = {
			{
				type = env.DRAW_TYPE_SOLID,
				--params_mask = 0x000000FF, -- "Which unique params are needed?"
			},
			{ type = env.DRAW_TYPE_SOLID, alpha = true }
		},
		on_collide = function(bx, by, is_x)
			-- Called on every player! Do not check against the local `tile`.
			local coins = world.get_params(bx, by)
			local p_coins = get_pwdata(player).coins

			return (p_coins >= coins
				and env.COLLISION_TYPE_NONE
				or env.COLLISION_TYPE_POSITION)
		end,
	}),
	make_coin_block({
		id = ID_COINGATE,
		tiles = {
			{ type = env.DRAW_TYPE_SOLID, alpha = true },
			{ type = env.DRAW_TYPE_SOLID }
		},
		on_collide = function(bx, by, is_x)
			-- Called on every player! Do not check against the local `tile`.
			local coins = world.get_params(bx, by)
			local p_coins = get_pwdata(player).coins

			return (p_coins < coins
				and env.COLLISION_TYPE_NONE
				or env.COLLISION_TYPE_POSITION)
		end,
	}),
}

env.register_pack({
	name = "coins",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_coins)
})

reg.change_blocks(blocks_coins)
