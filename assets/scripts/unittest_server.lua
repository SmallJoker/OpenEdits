--[[
Concept idea:

1. register events for coins and other per-player information
2. use on_player_join / on_player_leave to send the data to all relevant locations
]]

local old_event = env.on_player_event or (function() end)
env.on_player_event = function(event, arg)
	old_event(event, arg)

	print("[server event]", env.player:get_name(), event, arg)
	if event == "join" then
		feedback("J_SRV")
	end
	if event == "leave" then
		feedback("L_SRV")
	end
end
