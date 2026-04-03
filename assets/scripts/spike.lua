local get_pwdata = reg.get_pwdata

local ID_CHECKPOINT = 360
local ID_SPIKES = 361


local EV_CHECKPOINT
EV_CHECKPOINT = env.register_event(ID_CHECKPOINT, 0, env.PARAMS_TYPE_STR16,
	function(str)
		local i = string.find(str, ",", 1, true)
		local px, py = tonumber(string.sub(str, 1, i - 1)), tonumber(string.sub(str, i + 1))
		local fg, _, _ = env.world.get_block(px, py)
		if fg ~= ID_CHECKPOINT then
			return
		end

		local pw_data = get_pwdata(env.player)
		pw_data.checkpoint = { px, py }
	end
)

local EV_KILLED
EV_KILLED = env.register_event(ID_SPIKES, 0, env.PARAMS_TYPE_U8,
	function()
		local pw_data = get_pwdata(env.player)
		if pw_data.godmode then
			return
		end

		local pos = pw_data.checkpoint
		-- TODO: delay teleport by a few seconds
		if pos then
			env.player:send_teleport(pos[1], pos[2])
		else
			reg.respawn_player(env.player)
		end
	end
)

local old_on_block_place = env.on_block_place
env.on_block_place = function(x, y, fg, bg)
	old_on_block_place(x, y, fg, bg)

	if not fg then
		return
	end
	local old_id, old_tile, _ = env.world.get_block(x, y)
	if old_id ~= ID_CHECKPOINT then
		return
	end

	for _, p in ipairs(env.world.get_players()) do
		local pw_data = get_pwdata(p)
		local pos = pw_data.checkpoint
		if pos and pos[1] == x and pos[2] == y then
			pw_data.checkpoint = nil
		end
	end
end

-- Clear any checkpoint data (server (N) and client (1))
local old_on_world_data = env.on_world_data
env.on_world_data = function()
	old_on_world_data()

	for _, p in ipairs(env.world.get_players()) do
		get_pwdata(p).checkpoint = nil
	end
end

local function set_my_checkpoint(px, py)
	local pw_data = get_pwdata(env.player)
	if pw_data.godmode then
		return
	end

	if px then
		px = math.floor(px + 0.5)
		py = math.floor(py + 0.5)
	end

	local prev = pw_data.checkpoint
	if prev then
		env.world.set_tile(ID_CHECKPOINT, 0, env.world.PRT_ONE_BLOCK, prev[1], prev[2])
	end

	if px then
		env.world.set_tile(ID_CHECKPOINT, 1, env.world.PRT_ONE_BLOCK, px, py)
		env.send_event(EV_CHECKPOINT, px .. "," .. py)
	end
	pw_data.checkpoint = px and { px, py }
end

local blocks_def = {
	{
		id = ID_CHECKPOINT,
		tiles = {
			{ type = env.DRAW_TYPE_DECURATION },
			{ type = env.DRAW_TYPE_ACTION },
		},
		on_intersect_once = function(tile)
			if tile > 0 or not env.is_me() then
				return
			end
			set_my_checkpoint(env.player:get_pos())
		end
	},
	{
		id = ID_SPIKES,
		params = env.PARAMS_TYPE_U8,
		gui_def = {
			-- v1.6.0 backwards compat
			type = gui.ELMT_TEXT, text = "?",

			values = { rot = 0 },
			from_block = function(values, rot)
				values.rot = rot
			end,
			on_place = function(values, x, y)
				local fg = env.world.get_block(x, y)
				local rot = values.rot
				if fg == ID_SPIKES then
					-- inherit rotation + 1
					rot = env.world.get_params(x, y)
					rot = (rot + 1) % 4
				end

				gui.select_block(nil, rot)
			end,
		},

		on_intersect_once = function(tile)
			if env.is_me() then
				-- TODO: disable player controls
				env.send_event(EV_KILLED, 0)
			end
		end,
		on_intersect = function()
			local vx, vy = env.player:get_vel()
			env.player:set_vel(vx * 0.1, vy * 0.1)
		end,

		tiles = {
			{ type = env.DRAW_TYPE_DECURATION },
			{ type = env.DRAW_TYPE_DECURATION },
			{ type = env.DRAW_TYPE_DECURATION },
			{ type = env.DRAW_TYPE_DECURATION },
		},
		get_visuals = function(tile, rot)
			return rot, nil
		end,
	},
}

env.register_pack({
	name = "spike",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_def)
})

reg.change_blocks(blocks_def)

