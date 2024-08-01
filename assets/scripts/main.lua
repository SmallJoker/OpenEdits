env.include("constants.lua")

-------------- Client & server script

-- TODO: not implemented
local GRAVITY    = 100.0 -- m/s² for use in callbacks
local JUMP_SPEED =  30.0 -- m/s  for use in callbacks

env.event_handlers[1] = function(...)
	print("CALL", unpack({...}))
end

env.world.on_player_join = function()
	print("JOIN", env.player.get_name())
end

env.world.on_player_leave = function()
	print("LEAVE", env.player.get_name())
end

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
]]

assert(env.API_VERSION >= 2, "Script implementation is too old.")

if true then
	env.load_hardcoded_packs()
	--[[env.change_block(0, {
		on_placed = function()
			print("PLACED")
		end
	})]]
	return
end

reg = {}

function reg.table_to_pack_blocks(block_defs)
	local t = {}
	for _, v in pairs(block_defs) do
		t[#t + 1] = v.id
	end
	return t
end

function reg.change_blocks(block_defs)
	for _, v in pairs(block_defs) do
		env.change_block(v.id, v)
	end
end

local table_to_pack_blocks = reg.table_to_pack_blocks
local change_blocks = reg.change_blocks

env.include("keys_doors.lua")

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
	get_tile = function(bx, by)
		local num_u8_1, num_u8_2  = env.world.get_params(bx, by)
		if num_u8_1 then
			--env.world.set_params() server only
		end
		env.gui.append_text(-3, -5, "value")
		return 1
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

env.register_pack({
	name = "doors",
	default_type = env.DRAW_TYPE_SOLID,
	blocks = table_to_pack_blocks(reg.blocks_doors)
})
change_blocks(reg.blocks_doors)


---------- Action tab


local player = env.player
local world = env.world
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


env.register_pack({
	name = "keys",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = table_to_pack_blocks(reg.blocks_keys)
})
change_blocks(reg.blocks_keys)


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

env.include("coins.lua")


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
