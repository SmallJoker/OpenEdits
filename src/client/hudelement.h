#pragma once

#include <string>
#include <vector2d.h>

using namespace irr;

struct HudElement {
	enum Type {
		T_TEXT,
		T_MAX_INVALID,
	};

	HudElement(Type type);
	~HudElement();

	const Type type;
	core::vector2d<s16> pos;
	// alignment

	union {
		std::string *text;
		// new elements go here
	} params;
};

