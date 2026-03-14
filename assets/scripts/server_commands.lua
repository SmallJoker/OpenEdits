
assert(reg.respawn_player)
env.server.register_command("respawn", {
	description = "Respawns the current player",
	syntax = "", -- TODO: also respawn other players
	run = function(msg)
		reg.respawn_player(env.player)
		return true, "Respawned!"
	end
})
