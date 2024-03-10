#include "blockmanager.h"
#include "packet.h"
#include <ITexture.h>
#include <IVideoDriver.h>
#include <stdexcept>

BlockManager *g_blockmanager = nullptr;

BlockProperties::BlockProperties(BlockDrawType type)
{
	setTiles({ type });
}

BlockProperties::~BlockProperties()
{
	// No need to free the textures. The driver does that.
}


void BlockProperties::setTiles(std::vector<BlockDrawType> types)
{
	tiles.resize(types.size());

	for (size_t i = 0; i < tiles.size(); ++i) {
		tiles[i].type = types[i];
		tiles[i].is_known_tile = true;
	}
}

static BlockTile invalid_tile;

BlockTile BlockProperties::getTile(const Block b) const
{
	if (tiles.empty())
		return invalid_tile;
	return b.tile < tiles.size() ? tiles[b.tile] : tiles.back();
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

void BlockManager::read(Packet &pkt)
{
	ASSERT_FORCED(pkt.data_version != 0, "invalid proto ver");
	// TODO: How to read/write the physics functions?

	std::vector<bool> handled_props;
	handled_props.resize(m_props.size());

	while (true) {
		bid_t block_id = pkt.read<bid_t>();
		if (block_id == Block::ID_INVALID)
			break;

		// Get more space (should not be neccessary)
		if (m_props.size() <= block_id)
			m_props.resize(block_id * 2);

		auto &props = m_props[block_id];
		if (!props)
			props = new BlockProperties(BlockDrawType::Invalid);

		props->paramtypes = (BlockParams::Type)pkt.read<u8>();
		props->viscosity = pkt.read<float>();

		u8 num_tiles = pkt.read<u8>();
		props->tiles.resize(num_tiles);
		for (BlockTile &tile : props->tiles) {
			tile.type = (BlockDrawType)pkt.read<u8>();
			tile.texture = nullptr;
			pkt.read<u8>(); // texture offset
			tile.have_alpha = pkt.read<u8>();
		}

		if (block_id < handled_props.size())
			handled_props[block_id] = true;
	}

	for (size_t i = 0; i < handled_props.size(); ++i) {
		if (!handled_props[i]) {
			delete m_props[i];
			m_props[i] = nullptr;
		}
	}

	// Read packs
	for (auto p : m_packs)
		delete p;

	u8 num_packs = pkt.read<u8>();
	m_packs.resize(num_packs);
	for (size_t i = 0; i < num_packs; ++i) {
		BlockPack pack(pkt.readStr16()); // pack name
		pack.default_type = (BlockDrawType)pkt.read<u8>();

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

	// Cannot populate textures here! It must happen in the main thread
	m_populated = false;
}

void BlockManager::write(Packet &pkt) const
{
	ASSERT_FORCED(pkt.data_version != 0, "invalid proto ver");

	// Data of each block
	for (size_t i = 0; i < m_props.size(); ++i) {
		auto props = m_props[i];
		if (!props)
			continue;

		pkt.write<bid_t>(i);
		pkt.write((u8)props->paramtypes);
		pkt.write<float>(props->viscosity);

		pkt.write<u8>(props->tiles.size()); // amount of tiles
		for (BlockTile &tile : props->tiles) {
			pkt.write((u8)tile.type);
			pkt.write<u8>(0); // TODO: remove
			pkt.write<u8>(tile.have_alpha);
		}
	}
	pkt.write<bid_t>(Block::ID_INVALID); // terminator

	// Serialize all pack data (there are no gaps)
	pkt.write<u8>(m_packs.size());
	for (auto pack : m_packs) {
		pkt.writeStr16(pack->name);
		pkt.write((u8)pack->default_type);

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

void BlockManager::setDriver(video::IVideoDriver *driver)
{
	m_driver = driver;
	m_missing_texture = m_driver->getTexture("assets/textures/missing_texture.png");
}

static void split_texture(video::IVideoDriver *driver, BlockTile *tile, u8 texture_offset)
{
	auto dim = tile->texture->getOriginalSize();
	video::IImage *img = driver->createImage(tile->texture,
		core::vector2di(texture_offset * dim.Height, 0),
		core::dimension2du(dim.Height, dim.Height)
	);

	char buf[255];
	snprintf(buf, sizeof(buf), "%p__%i", tile->texture, (int)texture_offset);

	tile->texture = driver->addTexture(buf, img);
	img->drop();
}

void BlockManager::populateTextures()
{
	if (m_populated)
		return;

	ASSERT_FORCED(m_driver && m_missing_texture, "Missing driver");

	int count = 0;

	for (auto pack : m_packs) {
		if (pack->imagepath.empty())
			pack->imagepath = "assets/textures/pack_" + pack->name + ".png";

		// Assign texture ID and offset?
		video::ITexture *texture = m_driver->getTexture(pack->imagepath.c_str());

		core::dimension2du dim;
		int max_tiles = 0;

		if (texture) {
			dim = texture->getOriginalSize();
			max_tiles = dim.Width / dim.Height;
		}

		int texture_offset = 0;
		for (bid_t id : pack->block_ids) {
			auto prop = m_props[id];
			if (texture_offset < max_tiles) {
				for (BlockTile &tile : prop->tiles) {
					if (tile.type == BlockDrawType::Invalid)
						break;

					tile.texture = texture;
					split_texture(m_driver, &tile, texture_offset);
					if (tile.is_known_tile)
						texture_offset++;
				}

				if (prop->color == 0)
					prop->color = getBlockColor(prop->tiles[0]);
			}

			for (BlockTile &tile : prop->tiles) {
				if (tile.texture)
					continue;

				tile.texture = m_missing_texture;
				if (prop->color == 0)
					prop->color = 0xFFFF0000; // red
				fprintf(stderr, "BlockManager: Out-of-range texture for block_id=%d\n", id);
			}

			count++;
		}
	}

	m_populated = true;

	doPackPostprocess();
	printf("BlockManager: Registered textures of %d blocks in %zu packs\n", count, m_packs.size());
}

const BlockProperties *BlockManager::getProps(bid_t block_id) const
{
	if (block_id >= m_props.size())
		return nullptr;

	return m_props[block_id];
}

BlockProperties *BlockManager::getPropsForModification(bid_t block_id) const
{
	if (m_populated) {
		fprintf(stderr, "BlockManager: Cannot modify blocks. Already in use.\n");
		return nullptr;
	}

	if (block_id >= m_props.size())
		return nullptr;

	return m_props[block_id];
}


const BlockPack *BlockManager::getPack(const std::string &name) const
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

	// Calculate median color
	for (size_t y = 0; y < dim.Height; ++y)
	for (size_t x = 0; x < dim.Height; ++x) {
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
