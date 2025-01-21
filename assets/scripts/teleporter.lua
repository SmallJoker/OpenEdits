local ID_TELEPORTER = 242

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
			{ type = env.DRAW_TYPE_SOLID },
			{ type = env.DRAW_TYPE_SOLID },
			{ type = env.DRAW_TYPE_SOLID },
			{ type = env.DRAW_TYPE_SOLID }
		},
		on_intersect_once = function(tile)
			local px, py = env.player:get_pos()
			-- TODO: find correponding teleportation point
			env.player:set_pos(px, py)
		end
	}
}

env.register_pack({
	name = "teleporter",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_teleporter)
})

reg.change_blocks(blocks_teleporter)
