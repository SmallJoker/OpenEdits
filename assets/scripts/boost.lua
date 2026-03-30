local BOOST_V = 70 -- m/s
local GRAVITY = reg.GRAVITY_ACCEL
local player = env.player

local blocks_boost = {
	{
		id = 114, -- left
		on_intersect = function()
			player:set_acc(-GRAVITY, 0)
			player:set_vel(-BOOST_V, nil)
		end,
	},
	{
		id = 115, -- right
		on_intersect = function()
			player:set_acc(GRAVITY, 0)
			player:set_vel(BOOST_V, nil)
		end,
	},
	{
		id = 116, -- up
		on_intersect = function()
			player:set_acc(0, -GRAVITY)
			player:set_vel(nil, -BOOST_V)
		end,
	},
	{
		id = 117, -- down
		on_intersect = function()
			player:set_acc(0, GRAVITY)
			player:set_vel(nil, BOOST_V)
		end,
	},
}

env.register_pack({
	name = "boost",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_boost)
})
reg.change_blocks(blocks_boost)
