
-------------- provided by C++ (client + server-side)
--[[
env.player = <userdata>
env.API_VERSION = 1
env.register_block(def)
]]

-------------- Client & server script

GRAVITY    = 500 -- m/s² for use in callbacks
JUMP_SPEED =  60 -- m/s  for use in callbacks

env.register_block({
	id = 0,
	while_intersecting = function()
	end
})

env.register_block({
	id = 12,
	-- name = "boost:up"
	-- texture_start_index = 3,
	-- params (types)
	-- tiles (0 .. 7)
	while_intersecting = function()
		local px, py = env.player.get_pos()
		px = px + 12345
		py = py + 1
		env.player.set_pos(nil, py)

		px, py = env.player.get_pos()
		print(px, py)
	end
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
