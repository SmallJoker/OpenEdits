local SOUND_PIANO = "piano_c4.mp3"
local PIANO_KEY_NAMES = {
	-- ' as a shorter notation for # to fit everyhing in the overlay
	"C", "C'", "D", "D'", "E", "F", "F'", "G", "G'", "A", "A'", "B"
};

env.require_asset(SOUND_PIANO)

local function do_play_piano_note()
	if not gui then
		return
	end

	local px, py = env.player:get_pos()
	local note = env.world.get_params(px, py)

	local tone_diff = note + 48 - 60;
	local pitch = math.pow(2.0, tone_diff / 12)

	gui.play_sound(SOUND_PIANO, {
		pitch = pitch
	})
end

local blocks_hidden = {
	{
		id = 77, -- ID_PIANO
		params = env.PARAMS_TYPE_U8,
		on_intersect_once = do_play_piano_note,

		-- Appearance

		tiles = {
			{ type = env.DRAW_TYPE_ACTION, alpha = true },
		},
		overlay = {
			type = gui.TOVT_TEXT_BR,
			fg_color = 0xFFFFFFFF,
			bg_color = 0xFF7B31EA,
		},
		gui_def = {
			type = gui.ELMT_TABLE, grid = { 2, 1 }, fields = {
				{ type = gui.ELMT_TEXT, text = "Note" },
				{ type = gui.ELMT_INPUT, name = "note" },
			},
			values = { ["note"] = 12 },
			on_input = function(values, k, v)
				if k == "note" then
					v = tonumber(v) and v or values[k]
				end
				values[k] = v
			end,
			on_place = function(values, x, y)
				gui.select_block(nil, values.note)
			end,
		},
		get_visuals = function(tile, note)
			-- param = 0 : octave = 3, key = "C"
			local octave = math.floor(note / 12 + 3)
			return 0, PIANO_KEY_NAMES[note % 12 + 1] .. tostring(octave)
		end,

	},
}

env.register_pack({
	name = "music",
	default_type = env.DRAW_TYPE_ACTION,
	blocks = reg.table_to_pack_blocks(blocks_hidden)
})

reg.change_blocks(blocks_hidden)

