#include "blockmanager.h"
#include <ITexture.h>
#include <IVideoDriver.h>
#include <exception>

BlockManager::BlockManager()
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
}


void BlockManager::addNewPack(BlockPack *pack)
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
		auto prop = new BlockProperties();
		prop->pack = pack;
		prop->type = pack->default_type;
		m_props[id] = prop;
	}

	if (m_driver) {
		// Assign texture ID and offset?
		video::ITexture *image = m_driver->getTexture(pack->imagepath.c_str());
		auto dim = image->getOriginalSize();
		//int max_tiles = dim.Width / dim.Height;

		int i = 0;
		for (bid_t id : pack->block_ids) {
			auto prop = m_props[id];
			prop->texture = image;
			prop->texture_offset = i * dim.Height;
			i++;
		}
	}
}

BlockProperties *BlockManager::addToPack(BlockPack *pack, bid_t block_id, const std::string &imagepath)
{
	if (getProps(block_id))
		throw std::runtime_error("Block is already registered");

	// TODO
	return nullptr;
}

BlockProperties *BlockManager::getProps(bid_t block_id)
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

void BlockManager::ensurePropsSize(size_t n)
{
	if (n >= m_props.size())
		m_props.resize(n + 64, nullptr);
}

