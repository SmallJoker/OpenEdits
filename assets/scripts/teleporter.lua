local ID_TELEPORTER = 242
local player = env.player

local blocks_teleporter = {
	{
		id = ID_TELEPORTER,
		gui_def = {
			-- root element
			type = gui.ELMT_TABLE, grid = { 2, 2 }, fields = {
				{ type = gui.ELMT_TEXT, text = "ID" },
				{ type = gui.ELMT_INPUT, name = "id" },
				{ type = gui.ELMT_TEXT, text = "DST" },
				{ type = gui.ELMT_INPUT, name = "dst" },
			},
			values = { ["id"] = 1, ["dst"] = 2 },
			on_input = function(values, k, v)
				if k == "id" or k == "dst" then
					v = tonumber(v) and v or values[k]
				end
				values[k] = v
			end,
			on_place = function(values, x, y)
				local fg, tile, bg = env.world.get_block(x, y)
				local rot = 0
				if fg == ID_TELEPORTER then
					-- inherit rotation + 1
					rot, _, _ = env.world.get_params(x, y)
					rot = (rot + 1) % 4;
				end

				gui.select_block(nil, rot, values.id, values.dst)
			end,
		},
		params = env.PARAMS_TYPE_U8U8U8,
		tiles = {
			{ type = env.DRAW_TYPE_ACTION },
			{ type = env.DRAW_TYPE_ACTION },
			{ type = env.DRAW_TYPE_ACTION },
			{ type = env.DRAW_TYPE_ACTION }
		},
		on_intersect_once = function(_)
			if player_data[player:hash()].godmode then
				return -- no effect
			end

			local px, py = player:get_pos()
			local rot, _, dst = env.world.get_params(px, py)

			local list = env.world.get_blocks_in_range({
				return_pos = true,
				return_params = true,
			}, { ID_TELEPORTER }, env.world.PRT_ENTIRE_WORLD)

			local options = {}
			for i, v in ipairs(list) do
				local _, x2, y2, rot2, id2, dst2 = unpack(v)
				if dst == id2 then
					table.insert(options, i)
				end
			end
			if #options == 0 then
				return -- no destination
			end

			-- Destination teleportation point
			local i = (player:next_prn() % #options) + 1
			local _, x2, y2, rot2 = unpack(list[options[i]])
			player:set_pos(x2, y2)

			local rotdiff = (rot2 - rot + 4) % 4 -- 1 = 90° CW, 3 = 270° CW
			local vx, vy = player:get_vel()
			if rotdiff == 1 then -- 90° CW
				player:set_vel(-vy, vx)
			elseif rotdiff == 2 then -- 180°
				player:set_vel(-vx, -vy)
			elseif rotdiff == 3 then -- 90° CCW
				player:set_vel(vy, -vx)
			end
		end
	}
}

env.register_pack({
	name = "teleporter",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_teleporter)
})

reg.change_blocks(blocks_teleporter)
