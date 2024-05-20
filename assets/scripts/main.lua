
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

local function table_to_pack_blocks(block_defs)
	local t = {}
	for _, v in pairs(block_defs) do
		t[#t + 1] = v.id
	end
	return t
end

local function change_blocks(block_defs)
	for _, v in pairs(block_defs) do
		env.change_block(v.id, v)
	end
end


---------- Blocks tab


env.register_pack({
	name = "basic",
	default_type = env.DRAW_TYPE_SOLID,
	blocks = { 9, 10, 11, 12, 13, 14, 15 }
})

env.change_block(10, {
	-- blue block
	minimap_color = 0xFFFFFFFF,  -- AARRGGBB, white
})


---------- Action tab


local player = env.player
local blocks_action = {
	-- Cannot use indices: unordered `pairs` iteration.
	{
		id = 0,
		on_intersect = function()
			player.set_acc(0, GRAVITY)
		end,
	},
	{
		id = 1,
		on_intersect = function()
			player.set_acc(-GRAVITY, 0)
		end,
	},
	{
		id = 2,
		on_intersect = function()
			player.set_acc(0, -GRAVITY)
		end,
	},
	{
		id = 3,
		on_intersect = function()
			player.set_acc(GRAVITY, 0)
		end,
	},
	{
		id = 4,
		viscosity = 0.1,
		on_intersect = function()
			-- nop
		end,
	}
}

env.register_pack({
	name = "action",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = table_to_pack_blocks(blocks_action)
})

change_blocks(blocks_action)


---------- Decoration tab


env.register_pack({
	name = "spring",
	default_type = env.DRAW_TYPE_DECORATION,
	blocks = { 233, 234, 235, 236, 237, 238, 239, 240 }
})


---------- Backgrounds tab


env.register_pack({
	name = "simple",
	default_type = env.DRAW_TYPE_BACKGROUND,
	blocks = { 500, 501, 502, 503, 504, 505, 506 }
})
