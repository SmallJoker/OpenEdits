local player_data = assert(reg._player_data)
local player = env.player

local old_event = env.on_player_event or (function() end)
env.on_player_event = function(event, arg)
	print((env.server and "Server" or "Client"),
		"event:" .. event, env.player:get_name(), arg)

	local id = player:hash()
	if event == "join" then
		player_data[id] = {
			godmode = false,
			coins = 0,
		}
	end

	if event == "godmode" then
		-- TODO: sent too early
		local pd = player_data[id]
		if not pd then
			return
		end
		pd.godmode = arg
	end

	old_event(event, arg)

	if event == "leave" then
		player_data[id] = nil
	end
end
