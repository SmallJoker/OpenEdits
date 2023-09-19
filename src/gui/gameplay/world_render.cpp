#include "world_render.h"
#include "gameplay.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "core/blockmanager.h"
#include "gui/CBulkSceneNode.h"
#include <ICameraSceneNode.h>
#include <ISceneCollisionManager.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <SViewFrustum.h>

static float ZINDEX_SMILEY[2] = {
	0, // god off
	-0.3
};
static float ZINDEX_LOOKUP[(int)BlockDrawType::Invalid] = {
	0.1, // Solid
	0.1, // Action
	-0.1, // Decoration
	2.2, // Background
};

SceneWorldRender::SceneWorldRender(SceneGameplay* parent, Gui* gui)
{
	m_gameplay = parent;
	m_gui = gui;
}

SceneWorldRender::~SceneWorldRender()
{
	if (m_world_smgr) {
		m_world_smgr->clear();
		m_world_smgr->drop();
	}
}


void SceneWorldRender::draw()
{
	zoom_factor = 2.4f;

	if (!m_world_smgr) {
		m_world_smgr = m_gui->scenemgr->createNewSceneManager(false);
		//m_world_smgr = m_gui->scenemgr;
	}
	if (m_world_smgr != m_gui->scenemgr)
		m_world_smgr->clear();

	auto smgr = m_world_smgr;

	// Main node to keep track of all children
	m_blocks_node = smgr->addBillboardSceneNode(nullptr,
		core::dimension2d<f32>(0.01f, 0.01f),
		core::vector3df(0, 0, 0)
	);

	// Main node to keep track of all children
	m_players_node = smgr->addBillboardSceneNode(nullptr,
		core::dimension2d<f32>(0.01f, 0.01f),
		core::vector3df(0, 0, 0)
	);

	// Set up camera

	m_camera = smgr->addCameraSceneNode(nullptr);

	auto player = m_gui->getClient()->getMyPlayer();
	if (!!player) {
		m_camera_pos.X = player->pos.X *  10;
		m_camera_pos.Y = player->pos.Y * -10;
	}
	m_camera_pos.Z = -500.0f;
	setCamera(m_camera_pos);

	// TODO: Upon resize, the world sometimes blacks out, even though
	// the world scene nodes are added... ?!
	m_dirty_worldmesh = true;
}

void SceneWorldRender::step(float dtime)
{
	// Actually draw the world contents

	drawBlocksInView();
	updatePlayerPositions(dtime);

	{
		auto draw_area = m_gameplay->getDrawArea();
		auto old_viewport = m_gui->driver->getViewPort();
		m_gui->driver->setViewPort(draw_area);

		core::matrix4 proj;
		proj.buildProjectionMatrixOrthoLH(
			draw_area.getWidth() / zoom_factor,
			draw_area.getHeight() / zoom_factor,
			100, 1000);
		m_camera->setProjectionMatrix(proj);

		m_world_smgr->drawAll();

		m_gui->driver->setViewPort(old_viewport);
	}

	do {
		auto player = m_gui->getClient()->getMyPlayer();
		if (!player)
			break;

		m_camera_pos.X += ((player->pos.X *  10) - m_camera_pos.X) * 5 * dtime;
		m_camera_pos.Y += ((player->pos.Y * -10) - m_camera_pos.Y) * 5 * dtime;

		setCamera(m_camera_pos);
	} while (false);
}


core::line3df SceneWorldRender::getShootLine(core::vector2di mousepos)
{
	auto old_viewport = m_gui->driver->getViewPort();
	m_gui->driver->setViewPort(m_gameplay->getDrawArea());

	auto shootline = m_world_smgr
			->getSceneCollisionManager()
			->getRayFromScreenCoordinates(mousepos, m_camera);

	m_gui->driver->setViewPort(old_viewport);

	return shootline;
}


void SceneWorldRender::setCamera(core::vector3df pos)
{
	m_camera->setPosition(pos);
	pos.Z += 1000;
	m_camera->setTarget(pos);
	m_camera->updateAbsolutePosition();
}

void SceneWorldRender::drawBlocksInView()
{
	auto world = m_gui->getClient()->getWorld();
	if (!world)
		return;

	const int x_center = std::round(m_camera_pos.X / 10),
		y_center = std::round(-m_camera_pos.Y / 10);
	int x_extent = 18,
		y_extent = 12;

	{
		// Updated in the last draw cycle (no need to set viewport now)
		const auto &panes = m_camera->getViewFrustum()->planes;
		core::vector3df center = m_camera_pos;
		center.Z = 0;

		core::vector3df intersection_x, intersection_y;
		panes[scene::SViewFrustum::VF_RIGHT_PLANE].getIntersectionWithLine(
			center, core::vector3df(1, 0, 0), intersection_x
		);
		panes[scene::SViewFrustum::VF_BOTTOM_PLANE].getIntersectionWithLine(
			center, core::vector3df(0, 1, 0), intersection_y
		);
		// Change to -2 offset for debugging
		x_extent = std::ceil(intersection_x.X / 10) - x_center + 1;
		y_extent = std::ceil(-intersection_y.Y / 10) - y_center + 1;

		if (x_extent < 0 || y_extent < 0)
			return; // window being resized
	}

	//printf("center: %i, %i, %i, %i\n", x_center, y_center, x_extent, y_extent);

	auto player = m_gui->getClient()->getMyPlayer();
	if (!player)
		return;

	SimpleLock lock(world->mutex);
	const auto world_size = world->getSize();
	core::recti world_border(
		core::vector2di(0, 0),
		core::vector2di(world_size.X, world_size.Y)
	);

	{
		// Figure out whether we need to redraw
		core::recti required_area(
			core::vector2di(x_center - x_extent, y_center - y_extent),
			core::vector2di(x_center + x_extent, y_center + y_extent)
		);
		required_area.clipAgainst(world_border);

		core::recti clipped = m_drawn_blocks;
		clipped.clipAgainst(required_area); // overlapping area

		if (!m_dirty_worldmesh && clipped.getArea() >= required_area.getArea())
			return;

		m_dirty_worldmesh = false;
	}

	auto smgr = m_gui->scenemgr;
	m_blocks_node->removeAll();

	// Draw more than necessary to skip on render steps when moving only slightly
	m_drawn_blocks = core::recti(
		core::vector2di(x_center - x_extent - 2, y_center - y_extent - 2),
		core::vector2di(x_center + x_extent + 2, y_center + y_extent + 2)
	);
	m_drawn_blocks.clipAgainst(world_border);
	const auto upperleft = m_drawn_blocks.UpperLeftCorner; // move to stack
	const auto lowerright = m_drawn_blocks.LowerRightCorner;

	struct BulkData {
		CBulkSceneNode *node;
		bool is_solid;
	};
	std::map<bid_t, BulkData> bulk_map;

	// This is very slow. Isn't there a faster way to draw stuff?
	// also camera->setFar(-camera_pos.Z + 0.1) does not filter them out (bug?)
	for (int y = upperleft.Y; y <= lowerright.Y; y++)
	for (int x = upperleft.X; x <= lowerright.X; x++) {
		Block b;
		blockpos_t bp(x, y);
		if (!world->getBlock(bp, &b))
			continue;

		bool have_solid_above = false;
		do {
			if (b.id == 0)
				break;

			// MSVC is a bitch Part 2
			// Unique ID for each appearance type
			bid_t block_tile = (b.tile << 13) | b.id;

			auto it = bulk_map.find(block_tile);
			if (it == bulk_map.end()) {
				// Yet not cached: Add.

				const BlockProperties *props = g_blockmanager->getProps(b.id);
				BlockTile tile;
				if (props)
					tile = props->getTile(b);
				else
					tile.type = BlockDrawType::Solid;
				auto z = ZINDEX_LOOKUP[(int)tile.type];

				// New scene node
				BulkData d;
				d.is_solid = true;
				d.node = new CBulkSceneNode(m_blocks_node, smgr, -1,
					core::vector3df(0, 0, z),
					core::dimension2d<f32>(10, 10)
				);
				auto [it2, tmp] = bulk_map.insert({block_tile, d});
				d.node->drop();

				it = it2;

				// Set up scene node
				it->second.is_solid = assignBlockTexture(tile, it->second.node);
			}

			it->second.node->addTile({x, -y});
			have_solid_above = it->second.is_solid;

			BlockParams params;
			switch (b.id) {
				case Block::ID_COINDOOR:
				case Block::ID_COINGATE:
					if (b.tile != 0)
						break;
					if (!world->getParams(bp, &params) || params != BlockParams::Type::U8)
						break;

					{
						int required = params.param_u8;
						required -= player->coins;

						auto texture = m_gameplay->generateTexture(std::to_string(required), 0xFF000000, 0xFFFFFFFF);
						auto dim = texture->getOriginalSize();

						auto nb = smgr->addBillboardSceneNode(m_blocks_node,
							core::dimension2d<f32>((float)dim.Width / dim.Height * 5, 5),
							core::vector3df((x * 10) + 0, (y * -10) - 2, -0.05)
						);
						nb->getMaterial(0).Lighting = false;
						nb->getMaterial(0).setTexture(0, texture);
					}
			break;
			}
		} while (false);


		do {
			if (have_solid_above)
				break;

			auto it = bulk_map.find(b.bg);
			if (it == bulk_map.end()) {
				// Yet not cached: Add.

				const BlockProperties *props = g_blockmanager->getProps(b.bg);
				BlockTile tile;
				if (props)
					tile = props->tiles[0]; // backgrounds do not change
				auto z = ZINDEX_LOOKUP[(int)BlockDrawType::Background];

				// New scene node
				BulkData d;
				d.is_solid = false;
				d.node = new CBulkSceneNode(m_blocks_node, smgr, -1,
					core::vector3df(0, 0, z),
					core::dimension2d<f32>(10, 10)
				);

				auto [it2, tmp] = bulk_map.insert({b.bg, d});
				d.node->drop();

				it = it2;

				// Set up scene node
				assignBlockTexture(tile, it->second.node);
			}

			it->second.node->addTile({x, -y});
		} while (false);
	}
}


bool SceneWorldRender::assignBlockTexture(const BlockTile tile, scene::ISceneNode *node)
{
	bool is_opaque = false;

	auto &mat = node->getMaterial(0);
	mat.Lighting = false;
	mat.ZWriteEnable = video::EZW_AUTO;
	node->getMaterial(0).forEachTexture([](video::SMaterialLayer &layer) {
		layer.MinFilter = video::ETMINF_LINEAR_MIPMAP_NEAREST;
	});
	if (!tile.texture) {
		node->getMaterial(0).setTexture(0, g_blockmanager->getMissingTexture());
		return true;
	}

	if (tile.type == BlockDrawType::Action || tile.have_alpha)
		mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
	else if (tile.type == BlockDrawType::Decoration)
		mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	else
		is_opaque = true;

	node->getMaterial(0).setTexture(0, tile.texture);
	return is_opaque;
}


void SceneWorldRender::updatePlayerPositions(float dtime)
{
	auto smgr = m_world_smgr;
	auto smiley_texture = m_gui->driver->getTexture("assets/textures/smileys.png");
	auto godmode_texture = m_gui->driver->getTexture("assets/textures/god_aura.png");

	auto dim = smiley_texture->getOriginalSize();
	int texture_tiles = dim.Width / dim.Height;

	do {
		// Hide nametags after a certain duration
		// Nested because "getMyPlayer" contains a lock
		auto me = m_gui->getClient()->getMyPlayer();
		if (!me)
			break;

		if (me->vel.getLengthSQ() < 10 * 10)
			m_nametag_show_timer += dtime;
		else
			m_nametag_show_timer = 0;
	} while (0);

	std::list<scene::ISceneNode *> children = m_players_node->getChildren();
	auto players = m_gui->getClient()->getPlayerList();
	for (auto it : *players.ptr()) {
		auto player = dynamic_cast<LocalPlayer *>(it.second);

		core::vector2di bp(player->pos.X + 0.5f, player->pos.Y + 0.5f);
		if (!m_drawn_blocks.isPointInside(bp))
			continue;

		core::vector3df nf_pos(
			player->pos.X * 10,
			player->pos.Y * -10,
			ZINDEX_SMILEY[player->godmode]
		);

		s32 nf_id = player->getGUISmileyId();
		scene::ISceneNode *nf = nullptr;
		for (auto &c : children) {
			if (c && c->getID() == nf_id) {
				nf = c;
				c = nullptr; // mark as handled
			}
		}

		if (nf) {
			nf->setPosition(nf_pos);
		} else {
			nf = smgr->addBillboardSceneNode(m_players_node,
				core::dimension2d<f32>(15, 15),
				nf_pos,
				nf_id
			);
			nf->forEachMaterial([](video::SMaterial &mat){
				mat.Lighting = false;
				mat.ZWriteEnable = video::EZW_AUTO;
				mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			});
			nf->getMaterial(0).forEachTexture([](video::SMaterialLayer &layer) {
				layer.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
				layer.MagFilter = video::ETMAGF_LINEAR;
			});
			nf->getMaterial(0).setTexture(0, smiley_texture);

			// Add nametag
			auto nt_texture = m_gameplay->generateTexture(it.second->name);
			auto nt_size = nt_texture->getOriginalSize();
			auto nt = smgr->addBillboardSceneNode(nf,
				core::dimension2d<f32>(nt_size.Width * 0.4f, nt_size.Height * 0.4f),
				core::vector3df(0, -10, 0),
				nf_id + 1
			);
			nt->forEachMaterial([](video::SMaterial &mat){
				mat.Lighting = false;
				mat.ZWriteEnable = video::EZW_AUTO;
				//mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			});
			nt->getMaterial(0).setTexture(0, nt_texture);
		}

		if (player->smiley_id < texture_tiles) {
			// Assign smiley texture offset
			auto &mat = nf->getMaterial(0).getTextureMatrix(0);
			mat.setTextureTranslate(player->smiley_id / (float)texture_tiles, 0);
			mat.setTextureScale(1.0f / texture_tiles, 1);
		}

		scene::ISceneNode *ga = nullptr;
		for (auto &c : nf->getChildren()) {
			if (c->getID() == nf_id + 2) {
				ga = c;
				break;
			}
		}

		// Add godmode aura
		if (player->godmode != (!!ga)) {
			// Difference!
			if (player->godmode) {
				auto ga = smgr->addBillboardSceneNode(nf,
					core::dimension2d<f32>(18, 18),
					core::vector3df(0, 0, 0.1),
					nf_id + 2
				);

				ga->forEachMaterial([](video::SMaterial &mat){
					mat.Lighting = false;
					mat.ZWriteEnable = video::EZW_AUTO;
					mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
				});
				ga->getMaterial(0).setTexture(0, godmode_texture);
			} else {
				nf->removeChild(ga);
			}
		}

		for (auto c : nf->getChildren()) {
			auto id = c->getID();
			if (id == nf_id + 1) {
				// Nametag
				c->setVisible(m_nametag_show_timer > 1.0f);
			}
			// id + 2: effect
		}
	}

	for (auto c : children) {
		if (c)
			m_players_node->removeChild(c);
	}

	//printf("drawing %zu players\n", m_players->getChildren().size());
}
