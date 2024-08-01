local function dump(what, depth, seen)
	depth = depth or 0
	seen = seen or {}
	if type(what) ~= "table" then
		return tostring(what)
	end
	if seen[what] then
		return "<circular ref>"
	end
	seen[what] = true

	local out = {}
	local indent = string.rep("\t", depth)
	out[#out + 1] = "{"
	for k, v in pairs(what) do
		out[#out + 1] = indent .. "\t[" .. dump(k, 0, seen) .. "] = "
			.. dump(v, depth + 1, seen)
	end
	out[#out + 1] = indent .. "}"
	return table.concat(out, "\n")
end

----------- STARTUP -----------

if env.test_mode == "media" then
	-- Function requires media manager to be present!
	env.include("constants.lua")
end

env.world.on_player_join = function()
	print("JOIN", env.player.get_name())
end

env.world.on_player_leave = function()
	print("LEAVE", env.player.get_name())
end

env.register_pack({
	name = "action",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = { 0, 1, 2, 3, 4 }
})

env.register_pack({
	name = "basic",
	default_type = env.DRAW_TYPE_SOLID,
	blocks = { 9, 10, 11, 12, 13, 14, 15 }
})

--[[
local coins = env.register_variable(env.PARAMS_TYPE_U8, FLAG_IMMEDIATE + FLAG_PUBLIC)
loca value = env.player.get_var(coins)
env.player.set_var(coins, value + 1)

env.player.variables = {
	[player_id] = { [var_id] = 1 }
}
env.on_set_variable = function(id, ...)
	-- server: verify

	-- client: update immediately
end

client -> server -> client
]]

env.change_block(2, {
	-- params (types)
	-- tiles (0 .. 7)
	on_collide = function(bx, by, is_x)
		print("on_collide: block_id=2")
		return env.test_mode
			and env.COLLISION_TYPE_VELOCITY
			or env.COLLISION_TYPE_NONE
	end,
	on_intersect = function()
		local px, py = env.player.get_pos()
		if env.test_mode == "py set -1" then
			py = py + 1
			env.player.set_pos(nil, py)
			return
		end
		error("unhandled test_mode")
	end,
	on_intersect_once = function(tile)
		print("on_intersect_once: tile=" .. tile)
		if env.test_mode == "py set +1" then
			local px, py = env.player.get_pos()
			env.player.set_pos(nil, py + 1)
			return
		end
		error("unhandled test_mode")
	end,
})

-- event sender/receiver test
local EV = env.register_event(1003, 0, env.PARAMS_TYPE_STR16, env.PARAMS_TYPE_U8U8U8,
	function(...)
		local a, b = unpack({...})
		print("got event!", dump({...}), dump(a), dump(b))
	end
)

env.change_block(4, {
	on_intersect = function()
		print("send event!")
		env.send_event(EV, "hello world", 200, 10, 3)
	end
})

--print(dump(_G))
