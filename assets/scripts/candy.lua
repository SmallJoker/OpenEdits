local function make_oneway_block(id)
	return {
		id = id,
		tiles = { { type = env.DRAW_TYPE_DECORATION, alpha = true } },
		on_collide = function(bx, by, is_x)
			if is_x then
				-- Sideway gate
				local _, py = player:get_pos()
				local ctrl_jump = player:get_controls().jump
				if py == by and not ctrl_jump then
					return env.COLLISION_TYPE_POSITION
				end
			else -- y
				local _, py = player:get_pos()
				local _, vy = player:get_vel()
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
	blocks = reg.table_to_pack_blocks(blocks_candy)
})

reg.change_blocks(blocks_candy)
