--[[
Concept idea:

1. register events for coins and other per-player information
2. use on_player_join / on_player_leave to send the data to all relevant locations
]]


local old_join = env.server.on_player_join or (function() end)
env.server.on_player_join = function(...)
	print("JOIN", env.player.get_name())
	old_join(...)
	feedback("J_SRV")
end

local old_leave = env.server.on_player_leave or (function() end)
env.server.on_player_leave = function(...)
	print("LEAVE", env.player.get_name())
	old_leave(...)
	feedback("L_SRV")
end
