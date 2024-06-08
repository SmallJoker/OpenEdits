
-------------- provided by C++ (client + server-side)
--[[
env.player = <userdata>
env.API_VERSION = 1
env.test_mode = string/nil (used by unittest)
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

-- TODO: not implemented
local GRAVITY    = 100.0 -- m/s² for use in callbacks
local JUMP_SPEED =  30.0 -- m/s  for use in callbacks

--[[
To implement:
env.callbacks.on_join(function()
	player.set_physics({
		default_acceleration = num,       -- when no "on_collide" is defined
		acceleration_multiplicator = num, -- should affect the final acceleration
		control_acceleration = num,
		jump_speed = num
	})
end)


env.change_block(99, {
	events = {
		-- Triggered by client-sent "on touch" or similar
		testevent = function()
			world.set_tile(ID_DOOR, "all", 1)
		end
	},
	on_intersect_once = function()
		env.send_event("testevent")
	end
})
]]

assert(env.API_VERSION >= 2, "Script implementation is too old.")

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

---------- Parameters (TODO)

--[[
coins = env.new_parameter("coins", "player", env.PARAMTYPE_U32)
-- In a callback:
local val = coins.get()
coins.set(325) -- sends updates, if the server assigned the std::set<id> *ptr; (in Player)
coins.on_change(function()
	env.hud.set("da da da. aha. " .. coins.get())
end)


change_block({
	paramtype = env.PARAMTYPE_U8U8,
	test = function(bx, by)
		local num_u8_1, num_u8_2  = env.world.get_params(bx, by)
		if num_u8_1 then
			--env.world.set_params() server only
		end
	end
})
]]

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
		on_intersect_once = function()
			local px, py = player.get_pos()
			print("intersect", px, py)
		end,
		on_intersect = function()
			-- nop (overwrite block_id=0
		end,
	}
}

env.register_pack({
	name = "action",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = table_to_pack_blocks(blocks_action)
})

change_blocks(blocks_action)

local function make_oneway_block(id)
	return {
		id = id,
		tiles = { { type = env.DRAW_TYPE_DECORATION, alpha = true } },
		on_collide = function(bx, by, is_x)
			if is_x then
				-- Sideway gate
				local _, py = player.get_pos()
				local ctrl_jump = player.get_controls().jump
				if py == by and not ctrl_jump then
					return env.COLLISION_TYPE_POSITION
				end
			else -- y
				local _, py = player.get_pos()
				local _, vy = player.get_vel()
				-- normal step-up
				if vy >= 0 and py + 0.55 < by then
					return env.COLLISION_TYPE_POSITION
				end
			end
			return env.COLLISION_TYPE_NONE
		end
	}
end

local blocks_candy = {
	{
		id = 60,
		tiles = { { alpha = true } }
	},
	make_oneway_block(61),
	make_oneway_block(62),
	make_oneway_block(63),
	make_oneway_block(64),
	{
		id = 65,
		tiles = { { alpha = true } }
	},
	{
		id = 66,
		tiles = { { alpha = true } }
	},
	{
		id = 67,
		tiles = { { alpha = true } }
	},
}

env.register_pack({
	name = "candy",
	default_type = env.DRAW_TYPE_SOLID,
	blocks = table_to_pack_blocks(blocks_candy)
})

change_blocks(blocks_candy)


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
