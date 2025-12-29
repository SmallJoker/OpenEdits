
-------------- provided by C++ (client + server-side)
--[[
env.player = <userdata>
env.API_VERSION = 1
env.test_mode = string/nil (used by unittest)
]]

do
	-- blockmanager.h / BlockProperties::CollisionType
	env.COLLISION_TYPE_POSITION = 0
	env.COLLISION_TYPE_VELOCITY = 1
	env.COLLISION_TYPE_NONE = 2
end

do
	-- blockmanager.h / BlockDrawType
	env.DRAW_TYPE_SOLID = 0
	env.DRAW_TYPE_ACTION = 1
	env.DRAW_TYPE_DECORATION = 2
	env.DRAW_TYPE_BACKGROUND = 3
end

do
	-- blockparams.h / BlockParams::Type
	env.PARAMS_TYPE_NONE   = 0
	env.PARAMS_TYPE_STR16  = 1
	env.PARAMS_TYPE_U8     = 2
	env.PARAMS_TYPE_U8U8U8 = 3
end

do
	-- operators.h / PositionRange::Type
	local w = env.world
	w.PRT_ONE_BLOCK    = 0x00
	w.PRT_AREA         = 0x01
	w.PRT_CIRCLE       = 0x02
	w.PRT_ENTIRE_WORLD = 0x03

	-- operators.h / PositionRange::Operator
	w.PROP_SET         = 0x00
	w.PROP_ADD         = 0x10

	-- world.h / BlockUpdate
	w.ID_ERASE_BACKGROUND = 0x8000
end

do
	-- scriptevent.h
	env.SEF_HAVE_ACTOR = 0x8000
end

-- "env.gui": backwards compatibility
gui = gui or env.gui or {}


if gui then
	-- hudelement.h / HudElement::Type
	gui.HUDT_TEXT = 0

	-- guiscript.cpp
	gui.ELMT_TABLE   = 1
	gui.ELMT_TEXT    = 5
	gui.ELMT_INPUT   = 6

	-- blockmanager.h / TileOverlayType
	gui.TOVT_TEXT_BR = 0
end

local old_register = env.register_event
env.register_event = function(event_id, ...)
	local values = { ... }
	assert(type(values[#values]) == "function")
	env.event_handlers[event_id] = values[#values]
	values[#values] = nil
	return assert(old_register(event_id, unpack(values)), "fail")
end
