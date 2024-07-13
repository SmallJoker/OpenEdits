if env.test_mode == "media" then
	env.include("constants.lua")
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

-- Only necessary functions
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

--print(dump(_G))
