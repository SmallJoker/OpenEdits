#include "hudelement.h"
#include <stdexcept>

HudElement::HudElement(Type type) :
	type(type)
{
	switch (type) {
		case T_TEXT:
			params.text = new std::string();
			break;
		case T_MAX_INVALID:
			throw std::runtime_error("invalid hud element type");
			break;
	}
}

HudElement::~HudElement()
{
	switch (type) {
		case T_TEXT:
			delete params.text;
			break;
		case T_MAX_INVALID:
			break;
	}
}
