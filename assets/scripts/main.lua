-- v1.3.1 backwards compatibility
env.event_handlers = {}

if true then
	env.load_hardcoded_packs()
	return
end

assert(env.API_VERSION >= 4, "Script implementation is too old.")

env.include("constants.lua")
env.include("smileys.lua")

-------------- Client & server script

local GRAVITY    = 100.0 -- m/s² for use in callbacks
-- TODO: not implemented
local JUMP_SPEED =  30.0 -- m/s  for use in callbacks

if env.server then
	env.on_player_event = function(event)
		print("event:" .. event, env.player:get_name())
		if event == "join" then
			local names = {}
			for _, p in ipairs(env.server.get_players_in_world()) do
				names[#names + 1] = p:get_name()
			end
			print("List of players: " .. table.concat(names, ", "))
			return
		end
	end
end

if env.server then
	env.on_step = function(abstime) end
end

-- Keep track of player data
player_data = {}

local old_event = env.on_player_event or (function() end)
env.on_player_event = function(event, arg)
	old_event(event, arg)

	local id = env.player:hash()
	if event == "join" then
		player_data[id] = {
			coins = 0
		}
	end
	if event == "leave" then
		player_data[id] = nil
	end

	if not player_data[id] then
		-- godmode may be called too early
		return
	end
	if event == "godmode" then
		player_data[id].godmode = arg
	end
end

--[[
To implement:
scriptevent_handler_func = function()
	player:set_physics({
		default_acceleration = num,       -- when no "on_collide" is defined
		acceleration_multiplicator = num, -- should affect the final acceleration
		control_acceleration = num,
		jump_speed = num
	})
end
]]

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

local last_event_id = 0x1000
function reg.next_event_id(flags)
	while last_event_id < 0x2000 do
		last_event_id = last_event_id + 1
		if not env.event_handlers[last_event_id] then
			return last_event_id + flags
		end
	end
	assert(false)
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

env.include("keys_doors.lua")
env.register_pack({
	name = "doors",
	default_type = env.DRAW_TYPE_SOLID,
	blocks = reg.table_to_pack_blocks(reg.blocks_doors)
})
reg.change_blocks(reg.blocks_doors)

env.include("candy.lua")


---------- Action tab


local player = env.player
local world = env.world
local blocks_action = {
	-- Cannot use indices: unordered `pairs` iteration.
	{
		id = 0,
		on_intersect = function()
			player:set_acc(0, GRAVITY)
		end,
	},
	{
		id = 1,
		on_intersect = function()
			player:set_acc(-GRAVITY, 0)
		end,
	},
	{
		id = 2,
		on_intersect = function()
			player:set_acc(0, -GRAVITY)
		end,
	},
	{
		id = 3,
		on_intersect = function()
			player:set_acc(GRAVITY, 0)
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
	blocks = reg.table_to_pack_blocks(blocks_action)
})
reg.change_blocks(blocks_action)


env.register_pack({
	name = "keys",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(reg.blocks_keys)
})
reg.change_blocks(reg.blocks_keys)

env.include("coins.lua")
env.include("teleporter.lua")
env.include("hidden.lua")


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
