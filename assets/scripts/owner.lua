local ID_SPAWN = 255

local function get_random_spawn(player)
	local rand = player:next_prn()
	local list = env.world.get_blocks_in_range(
		{return_pos = true}, {ID_SPAWN}, env.world.PRT_ENTIRE_WORLD
	)
	local pos = list[(rand % #list) + 1]
	--print("teleport", player:get_name(), pos[2], pos[3])
	return pos[2], pos[3]
end

local old_event = env.on_player_event or (function() end)
env.on_player_event = function(event)
	if event == "prejoin" and env.server then
		env.player:send_teleport(get_random_spawn(env.player))
	end

	return old_event(event, arg)
end

local blocks_def = {
	{
		id = ID_SPAWN,
		tiles = {
			{ type = env.DRAW_TYPE_ACTION, alpha = true },
		},
	},
}

env.register_pack({
	name = "owner",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_def)
})
