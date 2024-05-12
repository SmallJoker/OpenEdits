
-------------- provided by C++ (client + server-side)
--[[
env.player = <userdata>
env.API_VERSION = 1
env.test_mode = string/nil (used by unittest)
env.register_block(def)
]]

-- BlockProperties::CollisionType
env.COLLISION_TYPE_POSITION = 0
env.COLLISION_TYPE_VELOCITY = 1
env.COLLISION_TYPE_NONE = 2

-- blockmanager BlockDrawType
env.DRAW_TYPE_SOLID = 0
env.DRAW_TYPE_ACTION = 1
env.DRAW_TYPE_DECORATION = 2
env.DRAW_TYPE_BACKGROUND = 3

-------------- Client & server script

local GRAVITY    = 100.0 -- m/s² for use in callbacks
local JUMP_SPEED =  30.0 -- m/s  for use in callbacks

local dbgprint = env.test_mode and print or function() end

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

env.change_block({
	id = 0,
	on_intersect = function()
		env.player.set_acc(0, GRAVITY)
	end,
})

env.change_block({
	id = 2,
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
