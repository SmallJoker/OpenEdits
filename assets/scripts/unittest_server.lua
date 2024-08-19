--[[
Concept idea:

1. register events for coins and other per-player information
2. use on_player_join / on_player_leave to send the data to all relevant locations
]]


local old_join = env.world.on_player_join
env.world.on_player_join = function(...)
	old_join(...)
	feedback("J_SRV")
end

local old_leave = env.world.on_player_leave
env.world.on_player_leave = function(...)
	old_leave(...)
	feedback("L_SRV")
end
