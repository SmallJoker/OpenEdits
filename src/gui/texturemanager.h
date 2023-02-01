#pragma once

#include "core/world.h"
#include <irrTypes.h>
#include <string>

namespace irr {
	class ITexture;
}

using namespace irr;

class Player;

struct BlockPackData {
	std::string imagepath;
	std::string name;

	struct {
		u16 id;
		BlockProperties props;
	} assignments[];
};

class TextureManager {
public:
	TextureManager();

	void addPack(const std::string &path);
	ITexture *getTexture(u16 block_id);

private:
};

