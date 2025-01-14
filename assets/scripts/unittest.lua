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

env.test_feedback = ""
function feedback(str)
	env.test_feedback = env.test_feedback .. (str .. ";")
end

if env.test_mode:find("const") then
	-- Function requires media manager to be present!
	env.include("constants.lua")
end

if env.test_mode:find("media") then
	env.include("unittest_server.lua", "server")
end

env.register_pack({
	name = "action",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = { 0, 1, 2, 3, 4 }
})

env.change_block(0, {
	on_intersect = function()
		env.player:set_acc(0, 100)
	end,
})

env.register_pack({
	name = "basic",
	default_type = env.DRAW_TYPE_SOLID,
	blocks = { 9, 10, 11, 12, 13, 14, 15 }
})

env.register_pack({
	name = "dev_blocks",
	default_type = env.DRAW_TYPE_SOLID,
	blocks = { 101, 102, 103, 104, 105, 106, 107 }
})

--[[
local coins = env.register_variable(env.PARAMS_TYPE_U8, FLAG_IMMEDIATE + FLAG_PUBLIC)
loca value = env.player:get_var(coins)
env.player:set_var(coins, value + 1)

env.player:variables = {
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
		local px, py = env.player:get_pos()
		if env.test_mode == "py set -1" then
			py = py + 1
			env.player:set_pos(nil, py)
			return
		end
		error("unhandled test_mode")
	end,
	on_intersect_once = function(tile)
		print("on_intersect_once: tile=" .. tile)
		if env.test_mode == "py set +1" then
			local px, py = env.player:get_pos()
			env.player:set_pos(nil, py + 1)
			return
		end
		error("unhandled test_mode")
	end,
})

-- event sender/receiver test
local EV_3
-- C++: unittest_script
-- Adwd an event to the queue and run it (locally)
EV_3 = env.register_event(1003, 0, env.PARAMS_TYPE_STR16, env.PARAMS_TYPE_U8U8U8,
	function(...)
		print("got EV_3!", dump({...}))
		local s, u1, u2, u3 = unpack({...})
		feedback(s .. u1 .. u2 .. u3)
	end
)

local EV_4
local EV_4_counter = 0
-- C++: test_playerref_scriptevents (server-side loopback)
-- Ping pong events
local function ev_4_broadcast()
	EV_4_counter = EV_4_counter + 1
	print("send EV_4 #" .. EV_4_counter .. " to " .. myplayerref:get_name())
	myplayerref:send_event(EV_4, "EV4.", EV_4_counter, 55, 77)
end

EV_4 = env.register_event(1004, 0, env.PARAMS_TYPE_STR16, env.PARAMS_TYPE_U8U8U8,
	function(...)
		print("got EV_4", dump({...}))
		local s, u1, u2, u3 = unpack({...})
		feedback(s .. u1 .. u2 .. u3)
		ev_4_broadcast()
	end
)

env.change_block(4, {
	-- EV_03
	on_intersect = function()
		print("send EV_3")
		env.send_event(EV_3, "hello world", 200, 10, 3)
	end,
	-- EV_04
	on_intersect_once = function()
		ev_4_broadcast()
	end
})


--print(dump(_G))


-- C++: test_script_world_interop
-- Run the callback to change the tile appearance (client-side only)
env.change_block(101, {
	tiles = {
		-- Same types: client may switch on its own.
		{ type = env.DRAW_TYPE_SOLID },
		{ type = env.DRAW_TYPE_SOLID },
	},
	on_collide = function(bx, by)
		print("collide with 101 at ", bx, by)
		local fg, tile, bg = env.world.get_block(bx, by)
		if tile == 0 then
			assert(env.world.set_tile(101, 1, env.world.PRT_ONE_BLOCK, bx, by) == true)
		end
		return env.COLLISION_TYPE_POSITION
	end
})

-- C++: test_script_world_interop
-- Get blocks in range, triggered by on_intersect_once
env.change_block(102, {
	tiles = {
		{ type = env.DRAW_TYPE_ACTION },
	},
	params = env.PARAMS_TYPE_U8U8U8,
	on_intersect_once = function()
		local list = env.world.get_blocks_in_range({
			return_pos = true,
			return_tile = true,
			return_params = true,
		}, {101, 102}, env.world.PRT_ENTIRE_WORLD)
		print(dump(list))
		feedback("called_102 " .. #list .. " " .. #list[1] .. " " .. #list[2])
	end
})

-- C++: setup_guiscript
-- Display a Lua-provided GUI
env.change_block(103, {
	gui_def = {
		-- root element
		type = gui.ELMT_TABLE, grid = { 2, 2 }, fields = {
			{ type = gui.ELMT_TEXT, text = "coins" },
			{ type = gui.ELMT_NUMERIC, name = "coins", default = 2 },
		},
		values = { ["coins"] = 0 }, -- to be filled by engine
		pre_place = function(values, id, ...)
			-- must match the "params" type
			gui.select_params(values.coins)
		end,
	}
})
