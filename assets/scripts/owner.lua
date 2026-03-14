local ID_SPAWN = 255

local function get_spawn_points()
	return env.world.get_blocks_in_range(
		{return_pos = true}, {ID_SPAWN}, env.world.PRT_ENTIRE_WORLD
	)
end

function reg.respawn_player(player, list)
	list = list or get_spawn_points()
	if #list == 0 then
		return
	end

	local pos = list[math.random(1, #list)]
	--print("teleport", player:get_name(), pos[2], pos[3])
	player:send_teleport(pos[2], pos[3])
end

local old_event = env.on_player_event
env.on_player_event = function(event, arg)
	if event == "prejoin" and env.server then
		reg.respawn_player(env.player)
	end

	return old_event(event, arg)
end

local old_on_world_data = env.on_world_data
env.on_world_data = function()
	old_on_world_data()

	if env.server then
		local list = get_spawn_points()
		for _, p in ipairs(env.world.get_players()) do
			reg.respawn_player(p, list)
		end
	end
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
