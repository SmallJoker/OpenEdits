
local dbgprint = env.test_mode and print or function() end

env.change_block(2, {
	-- name = "boost:up"
	-- texture_start_index = 3,
	-- params (types)
	-- tiles (0 .. 7)
	on_intersect = function()
		local px, py = env.player.get_pos()
		if env.test_mode == "set_pos" then
			px = px + 12345
			py = py + 1
			env.player.set_pos(nil, py)

			px, py = env.player.get_pos()
			print(px, py)
		end
		if not env.test_mode then
			env.player.set_acc(0, -GRAVITY)
		end
	end,
	on_collide = function(bx, by, is_x)
		dbgprint("on_colllide block_id=2")
		return env.test_mode
			and env.COLLISION_TYPE_VELOCITY
			or env.COLLISION_TYPE_NONE
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
