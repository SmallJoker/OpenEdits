assert(reg.respawn_player)
env.server.register_command("respawn", {
	description = "Respawns the current player",
	syntax = "", -- TODO: also respawn other players
	run = function(msg)
		local who = env.player
		if #msg > 0 and env.API_VERSION >= 9 then
			-- Teleport someone else
			local flags = env.player:get_flags()
			if bit.band(flags, env.PF_MASK_WORLD) == 0 then
				return false, "Insufficient permissions"
			end
			who = env.world.find_player(msg)
		end

		if not who then
			return false, "Unknown player"
		end
		reg.respawn_player(who)
		return true, "Respawned!"
	end
})
