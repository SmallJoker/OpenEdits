
-------------- provided by C++ (client + server-side)
--[[
env.player = <userdata>
env.PROTO_VER = 6
env.PROTO_VER_MIN = 5
env.get_handler = function() end -- to overwrite
]]

--------------  Client & server script

local registered_blocks = {}
local function register_block(def)
	local outdef = {
		name = def.name,
		id = def.id
	}
	assert(outdef.name or outdef.id) -- need one at least

	-- resolve compatibility functions
	for k, v in pairs(def) do
		local field = {}

		if type(v) == "table" then
			-- Craft a compat LUT
			for version_min, value in pairs(v) do
				-- TODO: check type of "value"
				field[version_min] = value
			end

			for version = 1, env.PROTO_VER_MAX do
				field[version] = field[version] or field[version - 1]
			end
		else
			field = v
		end

		outdef[k] = field
	end

	registered_blocks[def.id or def.name] = outdef
	registered_blocks[def.name or def.id] = outdef
end

-- The only function that matters to C++
-- retrieves the drawtype and callback functions
env.get_definition = function(block, version)
	-- function or value to use for physics etc
	local outdef = {}
	for k, v in pairs(registered_blocks[block]) do
		outdef[k] = type(v) == "table" and v[version] or v
	end
	return outdef
end

-------------- start of sent script
GRAVITY    = 500 -- m/s² for use in callbacks
JUMP_SPEED =  60 -- m/s  for use in callbacks

-- example block registration
register_block({
	name = "boost_up", id = 666,
	texture = "pack_basic.png",
	texture_start_index = 3,
	while_intersecting = {
		-- table indexed by protocol/API version
		[7] = function(pos, vel, acc) -- newest features
			vel.x = vel.x + 5
		end,
		[1] = function() -- legacy clients
		end
	},
	tiles = {
		{ -- tile index 1
			[7] = {
				drawtype = "decoration"
			},
			[1] = {
				drawtype = "solid",
				alpha = true
			}
		},
		{ -- tile index 2
			{ drawtype = "solid" }
		}
	}
})
