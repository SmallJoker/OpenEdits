#include "blockmanager.h"
#include "packet.h"
#include <ITexture.h>
#include <IVideoDriver.h>
#include <stdexcept>

BlockManager *g_blockmanager = nullptr;

BlockProperties::BlockProperties(BlockDrawType type)
{
	tiles[0] = BlockTile();
	tiles[0].type = type;

	tiles[1] = BlockTile();
}

BlockTile BlockProperties::getTile(const Block b) const
{
	return tiles[b.tile < MAX_TILES ? b.tile : (MAX_TILES - 1)];
}


// -------------- BlockManager public -------------

BlockManager::BlockManager() :
	m_fallback(BlockDrawType::Solid)
{
	m_props.resize(1100, nullptr);
}

BlockManager::~BlockManager()
{
	for (auto p : m_packs)
		delete p;
	m_packs.clear();

	for (auto p : m_props)
		delete p;
	m_props.clear();

	printf("BlockManager: Freed registered data\n");
}

void BlockManager::read(Packet &pkt, u16 protocol_version)
{
	// TODO: How to read/write the physics functions?
	struct Hardcoded {
		BP_STEP_CALLBACK(*step);
		BP_COLLIDE_CALLBACK(*onCollide);
	};
	std::map<bid_t, Hardcoded> hardcoded;

	for (auto &p : m_props) {
		if (p->step || p ->onCollide) {
			bid_t block_id = &p - &m_props[0];

			Hardcoded hc;
			hc.step = p->step;
			hc.onCollide = p->onCollide;

			hardcoded.emplace(block_id, hc);
		}

		// Reset all entries
		delete p;
		p = nullptr;
	}

	while (true) {
		bid_t block_id = pkt.read<bid_t>();
		if (block_id == Block::ID_INVALID)
			break;

		BlockProperties props(BlockDrawType::Invalid);
		props.paramtypes = (BlockParams::Type)pkt.read<u8>();
		props.viscosity = pkt.read<float>();

		u8 num_tiles = pkt.read<u8>();
		for (size_t t = 0; t < num_tiles; ++t) {
			auto &tile = props.tiles[t];
			tile.type = (BlockDrawType)pkt.read<u8>();
			tile.texture_offset = pkt.read<u8>();
			tile.have_alpha = pkt.read<u8>();
		}

		auto it = hardcoded.find(block_id);
		if (it != hardcoded.end()) {
			props.step = it->second.step;
			props.onCollide = it->second.onCollide;
		}

		// Get more space (should not be neccessary)
		if (m_props.size() <= block_id)
			m_props.resize(block_id * 2);

		m_props[block_id] = new BlockProperties(props);
	}

	// Read packs
	for (auto p : m_packs)
		delete p;

	u8 num_packs = pkt.read<u8>();
	m_packs.resize(num_packs);
	for (size_t i = 0; i < num_packs; ++i) {
		BlockPack pack(pkt.readStr16());

		pack.block_ids.resize(pkt.read<u8>());
		for (bid_t &block_id : pack.block_ids)
			pkt.read(block_id);

		m_packs[i] = new BlockPack(pack);
	}

	// Resolve pack data
	for (auto pack : m_packs) {
		for (bid_t block_id : pack->block_ids) {
			// BlockProperties is created based on this list,
			// hence it should never be nullptr (or error)
			auto props = m_props.at(block_id);
			if (!props)
				throw std::runtime_error("Missing props");

			props->pack = pack;
		}
	}

	// TODO: resolve all textures

}

void BlockManager::write(Packet &pkt, u16 protocol_version) const
{
	// Data of each block
	for (size_t i = 0; i < m_props.size(); ++i) {
		auto props = m_props[i];
		if (!props)
			continue;

		pkt.write<bid_t>(i);
		pkt.write((u8)props->paramtypes);
		pkt.write<float>(props->viscosity);

		u8 num_tiles = BlockProperties::MAX_TILES;
		pkt.write<u8>(num_tiles); // amount of tiles
		for (size_t t = 0; t < num_tiles; ++t) {
			auto &tile = props->tiles[t];
			pkt.write((u8)tile.type);
			pkt.write<u8>(tile.texture_offset);
			pkt.write<u8>(tile.have_alpha);
		}
	}
	pkt.write<bid_t>(Block::ID_INVALID); // terminator

	// Serialize all pack data (there are no gaps)
	pkt.write<u8>(m_packs.size());
	for (auto pack : m_packs) {
		pkt.writeStr16(pack->name);

		pkt.write<u8>(pack->block_ids.size());
		for (bid_t block_id : pack->block_ids)
			pkt.write(block_id);
	}
}


void BlockManager::registerPack(BlockPack *pack)
{
	if (getPack(pack->name))
		throw std::runtime_error("Pack already exists");

	if (pack->name.empty() || pack->block_ids.empty())
		throw std::runtime_error("Invalid pack information");

	bid_t max_id = 0;
	for (bid_t id : pack->block_ids) {
		if (getProps(id))
			throw std::runtime_error("Block is already registered");
		max_id = std::max(max_id, id);
	}

	m_packs.push_back(pack);

	// Register properties
	ensurePropsSize(max_id);
	for (bid_t id : pack->block_ids) {
		auto props = new BlockProperties(pack->default_type);
		props->pack = pack;
		m_props[id] = props;
	}
}

static void split_texture(video::IVideoDriver *driver, BlockTile *tile)
{
	auto dim = tile->texture->getOriginalSize();
	video::IImage *img = driver->createImage(tile->texture,
		core::vector2di(tile->texture_offset * dim.Height, 0),
		core::dimension2du(dim.Height, dim.Height)
	);

	std::string name = std::to_string((uint64_t)tile->texture);
	name.append("&&");
	name.append(std::to_string(tile->texture_offset));

	tile->texture = driver->addTexture(name.c_str(), img);
	tile->texture_offset = 0;
	img->drop();
}

void BlockManager::populateTextures(video::IVideoDriver *driver)
{
	if (!driver || m_missing_texture)
		return;

	int count = 0;
	m_missing_texture = driver->getTexture("assets/textures/missing_texture.png");
	m_driver = driver;

	for (auto pack : m_packs) {
		if (pack->imagepath.empty())
			pack->imagepath = "assets/textures/pack_" + pack->name + ".png";

		// Assign texture ID and offset?
		video::ITexture *texture = driver->getTexture(pack->imagepath.c_str());

		core::dimension2du dim;
		int max_tiles = 0;

		if (texture) {
			dim = texture->getOriginalSize();
			max_tiles = dim.Width / dim.Height;
		}

		int i = 0;
		for (bid_t id : pack->block_ids) {
			auto prop = m_props[id];
			if (i < max_tiles) {
				prop->tiles[0].texture = texture;
				u8 &offset = prop->tiles[0].texture_offset;

				if (offset == 0)
					offset = i; // take last position
				else
					i = offset; // Continue from this index

				split_texture(driver, &prop->tiles[0]);

				if (prop->tiles[1].type != BlockDrawType::Invalid) {
					prop->tiles[1].texture = texture;
					// Relative offset from parent
					u8 &offset = prop->tiles[1].texture_offset;
					if (offset == 0)
						offset = ++i; // next texture. !! increment !!
					else
						offset += i; // specified offset

					split_texture(driver, &prop->tiles[1]);
				}

				if (prop->color == 0)
					prop->color = getBlockColor(prop->tiles[0]);
				i++;
			}
			if (!prop->tiles[0].texture) {
				prop->tiles[0].texture = m_missing_texture;
				prop->tiles[1].texture = m_missing_texture;

				if (prop->color == 0)
					prop->color = 0xFFFF0000; // red
				fprintf(stderr, "BlockManager: Out-of-range texture for block_id=%d\n", id);
			}
			count++;
		}
	}

	printf("BlockManager: Registered textures of %d blocks in %zu packs\n", count, m_packs.size());
}

const BlockProperties *BlockManager::getProps(bid_t block_id) const
{
	if (block_id >= m_props.size())
		return nullptr;

	return m_props[block_id];
}

BlockPack *BlockManager::getPack(const std::string &name)
{
	for (auto p : m_packs) {
		if (p->name == name)
			return p;
	}
	return nullptr;
}

// -------------- BlockManager private -------------

void BlockManager::ensurePropsSize(size_t n)
{
	if (n >= m_props.size())
		m_props.resize(n + 64, nullptr);
}

u32 BlockManager::getBlockColor(const BlockTile tile) const
{
	auto dim = tile.texture->getOriginalSize();
	auto texture = tile.texture;

	void *data = texture->lock(video::ETLM_READ_ONLY);

	video::IImage *img = m_driver->createImageFromData(
		texture->getColorFormat(),
		texture->getOriginalSize(),
		data, true, false
 	);

	std::map<video::SColor, int> histogram;

	const float h_levels = 6 * 10,
		s_levels = 8,
		v_levels = 4;

	const size_t x_start = dim.Height * tile.texture_offset;
	const size_t x_stop = x_start + dim.Height;

	// Calculate median color
	for (size_t y = 0; y < dim.Height; ++y)
	for (size_t x = x_start; x < x_stop; ++x) {
		video::SColor color = img->getPixel(x, y);
		if (color.getAlpha() < 127)
			continue;

		// RGB -> HSV: http://www.easyrgb.com/en/math.php#text20
		float r = color.getRed() / 255.0f,
			g = color.getGreen() / 255.0f,
			b = color.getBlue() / 255.0f;

		float cmin = std::min(r, std::min(g, b)),
			cmax = std::max(r, std::max(g, b));
		float delta = cmax - cmin;

		float h = 0,
			s = 0,
			v = cmax;

		if (delta != 0) {
			s = delta / cmax;

			float dr = (((cmax - r) / 6) + (delta / 2)) / delta,
				dg = (((cmax - g) / 6) + (delta / 2)) / delta,
				db = (((cmax - b) / 6) + (delta / 2)) / delta;

			if (r == cmax)
				h = db - dg;
			else if (g == cmax)
				h = (1.0f / 3) + dr - db;
			else if (b == cmax)
				h = (2.0f / 3) + dg - dr;

			if (h < 0)
				h += 1;
			if (h > 1)
				h -= 1;
		}

		video::SColor entry(
			255,
			std::round(h * h_levels),
			std::round(s * s_levels),
			std::round(v * v_levels)
		);
		histogram[entry]++;
	}

	// Get dominant HSV
	video::SColor color = 0;
	int highest = 0;
	for (auto it : histogram) {
		if (it.second <= highest)
			continue;

		color = it.first;
		highest = it.second;
	}

	{
		// Convert back to RGB
		float h = color.getRed() / h_levels,
			s = color.getGreen() / s_levels,
			v = color.getBlue() / v_levels;
		float r, g, b;
		if (s == 0) {
			r = g = b = v;
		} else {
			float h6 = h * 6;
			if (h6 == 6)
				h6 = 0;
			float i = std::floor(h6);
			float x1 = v * (1 - s),
				x2 = v * (1 - s * (h6 - i)),
				x3 = v * (1 - s * (1 - (h6 - i)));

			if      (i == 0) { r = v  ; g = x3 ; b = x1; }
			else if (i == 1) { r = x2 ; g = v  ; b = x1; }
			else if (i == 2) { r = x1 ; g = v  ; b = x3; }
			else if (i == 3) { r = x1 ; g = x2 ; b = v;  }
			else if (i == 4) { r = x3 ; g = x1 ; b = v;  }
			else             { r = v  ; g = x1 ; b = x2; }
		}
		color = video::SColor(255, r * 255, g * 255, b * 255);
	}

	img->drop();
	texture->unlock();

	return color.color;
}
