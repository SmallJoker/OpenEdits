#include "world_render.h"
#include "gameplay.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "core/blockmanager.h"
#include "core/packet.h"
#include "gui/CBulkSceneNode.h"
#include <ICameraSceneNode.h>
#include <ISceneCollisionManager.h>
#include <ISceneManager.h>
#include <IVideoDriver.h>
#include <SViewFrustum.h>

#if 1
	#define SANITY_LOG(...) printf(__VA_ARGS__)
#else
	#define SANITY_LOG(...) do {} while (0)
#endif

#if 0
	#define DEBUG_LOG(...) printf(__VA_ARGS__)
#else
	#define DEBUG_LOG(...) do {} while (0)
#endif



// negative = towards camera
static float ZINDEX_SMILEY[2] = {
	0, // god off
	-3
};
static float ZINDEX_LOOKUP[(int)BlockDrawType::Invalid] = {
	2, // Solid
	2, // Action
	-1, // Decoration
	5, // Background
};
static float ZINDEX_SHADOW = 3;

SceneWorldRender::SceneWorldRender(SceneGameplay *parent, Gui *gui)
{
	m_gameplay = parent;
	m_gui = gui;

	m_tex_shadow = gui->driver->getTexture("assets/textures/shadow.png");
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
	forceRedraw();
}

void SceneWorldRender::step(float dtime)
{
	do {
		// Update camera position before rendering
		auto player = m_gui->getClient()->getMyPlayer();
		if (!player)
			break;

		float x = player->pos.X *  10;
		float y = player->pos.Y * -10;
		if (player->did_jerk) {
			m_camera_pos.X = x;
			m_camera_pos.Y = y;
		} else {
			// Exponential-like interpolation
			m_camera_pos.X += (x - m_camera_pos.X) * 5 * dtime;
			m_camera_pos.Y += (y - m_camera_pos.Y) * 5 * dtime;
		}

		setCamera(m_camera_pos);
	} while (false);

	// Actually draw the world contents

	drawBlocksInView();
	updatePlayerPositions(dtime);

	if (zoom_factor < 1.0f)
		zoom_factor = 1.0f;
	if (zoom_factor > 5.0f)
		zoom_factor = 5.0f;


	{
		auto draw_area = m_gameplay->getDrawArea();
		auto old_viewport = m_gui->driver->getViewPort();
		m_gui->driver->setViewPort(draw_area);

		core::matrix4 proj;
		proj.buildProjectionMatrixOrthoLH(
			draw_area.getWidth() / zoom_factor,
			draw_area.getHeight() / zoom_factor,
			100, 1000);
		m_camera->setProjectionMatrix(proj, true);

		m_world_smgr->drawAll();

		m_gui->driver->setViewPort(old_viewport);
	}
}


core::line3df SceneWorldRender::getShootLine(core::vector2di mousepos)
{
	auto old_viewport = m_gui->driver->getViewPort();
	m_gui->driver->setViewPort(m_gameplay->getDrawArea());

	auto shootline = m_world_smgr
			->getSceneCollisionManager()
			->getRayFromScreenCoordinates(mousepos, m_camera);

	//printf("start %.1f,%.1f,%.1f\n", shootline.start.X, shootline.start.Y, shootline.start.Z);
	//printf("  end %.1f,%.1f,%.1f\n\n", shootline.end.X, shootline.end.Y, shootline.end.Z);
	m_gui->driver->setViewPort(old_viewport);

	return shootline;
}

static core::recti rect_u16_to_recti(core::rect<u16> inp)
{
	return core::recti(
		inp.UpperLeftCorner.X,  inp.UpperLeftCorner.Y,
		inp.LowerRightCorner.X, inp.LowerRightCorner.Y
	);
}

void SceneWorldRender::forceRedraw()
{
	m_drawn_rect = core::recti(0,0,0,0);
}

void SceneWorldRender::setCamera(core::vector3df pos)
{
	m_camera->setPosition(pos);
	pos.Z += 1000;
	m_camera->setTarget(pos);
	m_camera->updateAbsolutePosition();
}

struct BlockDrawData {
	struct BulkData {
		CBulkSceneNode *node = nullptr;
		bool is_solid = false;
	};

	/// NOTE: Ensure there are no conflicts between the main block and the overlay!
	static inline size_t hash(bid_t block_id, size_t payload)
	{
		return (payload << 16) | block_id;
	}

	std::map<size_t, BulkData> bulk_map;

	LocalPlayer *player = nullptr;
	World *world = nullptr;

	blockpos_t pos;
	Block b;
	BulkData *bulk = nullptr;
};

void SceneWorldRender::drawBlocksInView()
{
	Client *client = m_gui->getClient();
	auto world = client->getWorld();
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

	// TODO: It is only necessary to update Block IDs that were invalidated by
	// 'env.world.update_tiles'. That will pay out in large worlds.
	client->updateAllBlockTiles(false);

	SimpleLock lock(world->mutex);
	const auto world_size = world->getSize();
	const core::recti world_border(
		core::vector2di(0, 0),
		core::vector2di(world_size.X, world_size.Y)
	);

	/// Whether we rendered enough in the last iteration
	bool all_visible;
	{
		core::recti visible_rect(
			core::vector2di(x_center - x_extent, y_center - y_extent),
			core::vector2di(x_center + x_extent, y_center + y_extent)
		);
		visible_rect.clipAgainst(world_border);

		core::recti clipped = m_drawn_rect;
		clipped.clipAgainst(visible_rect); // overlapping area

		all_visible = clipped.getArea() >= visible_rect.getArea();
	}

	/// Whether the rendered blocks changed
	bool blocks_modified = world->modified_rect.isValid();
	if (blocks_modified) {
		core::recti modified = rect_u16_to_recti(world->modified_rect);
		world->modified_rect = World::make_rect_not_modified();
		DEBUG_LOG("rect: %d,%d,%d,%d\n",
			modified.UpperLeftCorner.X, modified.UpperLeftCorner.Y,
			modified.LowerRightCorner.X, modified.LowerRightCorner.Y
		);

		blocks_modified = modified.isRectCollided(m_drawn_rect);
	}

	DEBUG_LOG("draw: modified=%d, all_visible=%d\n", blocks_modified, all_visible);
	if (!blocks_modified && all_visible)
		return;

	m_blocks_node->removeAll();

	// Draw more than necessary to skip on render steps when moving only slightly
	m_drawn_rect = core::recti(
		core::vector2di(x_center - x_extent - 2, y_center - y_extent - 2),
		core::vector2di(x_center + x_extent + 2, y_center + y_extent + 2)
	);
	m_drawn_rect.clipAgainst(world_border);
	const auto upperleft = m_drawn_rect.UpperLeftCorner; // move to stack
	const auto lowerright = m_drawn_rect.LowerRightCorner;

	const bool is_hardcoded = g_blockmanager->isHardcoded();

	BlockDrawData bdd;
	bdd.player = client->getMyPlayer().ptr();
	bdd.world = world.get();

	// This is very slow. Isn't there a faster way to draw stuff?
	// also camera->setFar(-camera_pos.Z + 0.1) does not filter them out (bug?)
	for (int y = upperleft.Y; y <= lowerright.Y; y++)
	for (int x = upperleft.X; x <= lowerright.X; x++) {
		bdd.pos = blockpos_t(x, y);
		if (!world->getBlock(bdd.pos, &bdd.b))
			continue;

		// Let's hope those two get "optimized away"
		blockpos_t &bp = bdd.pos;
		Block &b = bdd.b;

		bdd.bulk = nullptr;

		do {
			// Unique ID for each appearance type
			size_t tile_hash = b.tile;

			if (is_hardcoded) {
				if (b.id == Block::ID_SECRET && b.tile == 0)
					break;

				if (b.id == Block::ID_BLACKFAKE && b.tile == 0) {
					bdd.b.id = Block::ID_BLACKREAL;
					tile_hash = 0;
				}

				if (b.id == Block::ID_TEXT) {
					tile_hash = 0;
					BlockParams params;
					world->getParams(bp, &params);
					if (params != BlockParams::Type::Text)
						break;

					Packet pkt;
					params.write(pkt);
					tile_hash = crc32_z(0, pkt.data(), pkt.size());
				}
			} else {
				// Apply visual override
				const auto props = g_blockmanager->getProps(b.id);
				if (props) {
					const auto vo = props->getTile(b).visual_override;
					if (vo.enabled) {
						//printf("apply override id=%d tile=%d\n", b.id, b.tile);
						b.id = vo.id;
						b.tile = vo.tile;
						tile_hash = vo.tile;
					}
				}
			}

			if (b.id == 0)
				break;

			size_t hash_node_id = BlockDrawData::hash(bdd.b.id, tile_hash);
			bdd.bulk = &bdd.bulk_map[hash_node_id];
			if (!bdd.bulk->node) {
				// Yet not cached: Add.
				assignNewForeground(bdd);
			}

			bdd.bulk->node->addTile({x, -y});

			drawBlockParams(bdd);
		} while (false);


		if (bdd.bulk && bdd.bulk->is_solid) {
			// solid above; no background needed. Add shadow instead.
			bdd.bulk = &bdd.bulk_map[SIZE_MAX];
			if (!bdd.bulk->node) {
				assignNewShadow(bdd);
			}

			bdd.bulk->node->addTile({x, -y});
		} else {
			bdd.bulk = &bdd.bulk_map[b.bg];
			if (!bdd.bulk->node) {
				// Yet not cached: Add.
				assignNewBackground(bdd);
			}

			bdd.bulk->node->addTile({x, -y});
		}
	}
}

static const core::dimension2d<f32> DEFAULT_TILE_SIZE(10, 10);


void SceneWorldRender::assignNewForeground(BlockDrawData &bdd)
{
	auto smgr = m_gui->scenemgr;

	const BlockProperties *props = g_blockmanager->getProps(bdd.b.id);
	BlockTile tile;
	if (props)
		tile = props->getTile(bdd.b);
	else
		tile.type = BlockDrawType::Solid;
	auto z = ZINDEX_LOOKUP[(int)tile.type];

	// New scene node
	bdd.bulk->node = new CBulkSceneNode(m_blocks_node, smgr, -1,
		core::vector3df(0, 0, z),
		DEFAULT_TILE_SIZE
	);
	bdd.bulk->node->drop();
	bdd.bulk->is_solid = assignBlockTexture(tile, bdd.bulk->node);

	//bdd.bulk->node->setDebugDataVisible(scene::EDS_BBOX);
}

void SceneWorldRender::assignNewShadow(BlockDrawData &bdd)
{
	bdd.bulk->node = new CBulkSceneNode(m_blocks_node, m_gui->scenemgr, -1,
		core::vector3df(1.5f, -1.5f, ZINDEX_SHADOW),
		DEFAULT_TILE_SIZE
	);
	bdd.bulk->node->drop();

	auto &mat = bdd.bulk->node->getMaterial(0);
	mat.ZWriteEnable = video::EZW_AUTO;
	mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
	mat.setTexture(0, m_tex_shadow);
}

void SceneWorldRender::assignNewBackground(BlockDrawData &bdd)
{
	auto smgr = m_gui->scenemgr;

	const BlockProperties *props = g_blockmanager->getProps(bdd.b.bg);
	BlockTile tile;
	if (props)
		tile = props->tiles[0]; // backgrounds cannot change (yet?)
	auto z = ZINDEX_LOOKUP[(int)BlockDrawType::Background];

	// New scene node
	bdd.bulk->node = new CBulkSceneNode(m_blocks_node, smgr, -1,
		core::vector3df(0, 0, z),
		DEFAULT_TILE_SIZE
	);
	bdd.bulk->node->drop();

	// Set up scene node
	assignBlockTexture(tile, bdd.bulk->node);
}


void SceneWorldRender::drawBlockParams(BlockDrawData &bdd)
{
	if (!g_blockmanager->isHardcoded()) {
		TileCacheManager &tcache = m_gui->getClient()->getTileCacheMgr();
		const Block *b = bdd.world->getBlockPtr(bdd.pos);

		const TileCacheEntry entry = tcache.getOrCache(b);
		if (entry.overlay.empty())
			return;

		DEBUG_LOG("ADD OVERLAY @ %d,%d str=%s\n",
			bdd.pos.X, bdd.pos.Y, entry.overlay.c_str()
		);

		const size_t upper_hash = 0
			| (size_t)0xFF // any tile
			| std::hash<std::string>{}(entry.overlay) << 8;
		const size_t hash_node_id = BlockDrawData::hash(bdd.b.id, upper_hash);

		auto overlay = &bdd.bulk_map[hash_node_id];
		if (!overlay->node) {
			const BlockProperties *props = g_blockmanager->getProps(bdd.b.id);
			auto texture = m_gameplay->generateTexture(
				entry.overlay.c_str(),
				props->overlay.fg_color,
				props->overlay.bg_color
			);
			overlay->node = drawBottomLeftText(texture);
		}
		overlay->node->addTile({bdd.pos.X, -bdd.pos.Y});
		return;
	}

	BlockParams params;
	switch (bdd.b.id) {
		case Block::ID_PIANO:
			if (!bdd.world->getParams(bdd.pos, &params) || params != BlockParams::Type::U8) {
				SANITY_LOG("Invalid %s at pos=(%i,%i)\n", "Piano note", bdd.pos.X, bdd.pos.Y);
				break;
			}

			{
				// see SceneGameplay::handleOnTouchBlock
				uint8_t note = params.param_u8;
				std::string note_str = "??";
				SceneGameplay::pianoParamToNote(note, &note_str);

				size_t hash_node_id = BlockDrawData::hash(bdd.b.id, note + 8);
				auto overlay = &bdd.bulk_map[hash_node_id];
				if (!overlay->node) {
					auto texture = m_gameplay->generateTexture(note_str.c_str(), 0xFFFFFFFF, 0xFF7B31EA);
					overlay->node = drawBottomLeftText(texture);
				}

				overlay->node->addTile({bdd.pos.X, -bdd.pos.Y});
			}
			break;
		case Block::ID_COINDOOR:
		case Block::ID_COINGATE:
			if (bdd.b.tile != 0)
				break;
			if (!bdd.world->getParams(bdd.pos, &params) || params != BlockParams::Type::U8) {
				SANITY_LOG("Invalid %s at pos=(%i,%i)\n", "coin door/gate", bdd.pos.X, bdd.pos.Y);
				break;
			}

			{
				uint8_t required = params.param_u8;
				required -= bdd.player->coins;

				size_t hash_node_id = BlockDrawData::hash(bdd.b.id, required + 8);
				auto overlay = &bdd.bulk_map[hash_node_id];
				if (!overlay->node) {
					auto texture = m_gameplay->generateTexture(std::to_string(required), 0xFF000000, 0xFFEECC00);
					overlay->node = drawBottomLeftText(texture);
				}

				overlay->node->addTile({bdd.pos.X, -bdd.pos.Y});
			}
			break;
		case Block::ID_TELEPORTER:
			if (!bdd.world->getParams(bdd.pos, &params) || params != BlockParams::Type::Teleporter) {
				SANITY_LOG("Invalid %s at pos=(%i,%i)\n", "teleporter", bdd.pos.X, bdd.pos.Y);
				break;
			}

			{
				uint8_t tp_id = params.teleporter.id;

				size_t hash_node_id = BlockDrawData::hash(bdd.b.id, tp_id + 8);
				auto overlay = &bdd.bulk_map[hash_node_id];
				if (!overlay->node) {
					auto texture = m_gameplay->generateTexture(std::to_string(tp_id), 0xFF000000, 0);
					overlay->node = drawBottomLeftText(texture);
				}

				overlay->node->addTile({bdd.pos.X, -bdd.pos.Y});
			}
			break;
		case Block::ID_TEXT: {
			bdd.world->getParams(bdd.pos, &params);
			if (params != BlockParams::Type::Text) {
				SANITY_LOG("Invalid %s at pos=(%i,%i)\n", "text", bdd.pos.X, bdd.pos.Y);
				break;
			}

			video::ITexture *txt = m_gameplay->generateTexture(*params.text, 0xFFFFFFFF, 0x77000000);
			auto dim = txt->getOriginalSize();

			auto node = bdd.bulk->node;
			node->getMaterial(0).setTexture(0, txt);
			node->setVertexSize(core::dimension2df((float)dim.Width / dim.Height * 8, 8));
		}
	break;
	}
}

CBulkSceneNode *SceneWorldRender::drawBottomLeftText(video::ITexture *texture)
{
	auto dim_i = texture->getOriginalSize();
	core::dimension2df dim;
	dim.Height = 5;
	dim.Width = (float)dim_i.Width / dim_i.Height * dim.Height;

	// Align right
	auto node = new CBulkSceneNode(m_blocks_node, m_gui->scenemgr, -1,
		core::vector3df(DEFAULT_TILE_SIZE.Width - dim.Width - 1, 1, 0.5),
		DEFAULT_TILE_SIZE
	);
	node->drop();

	node->setVertexSize(dim);
	node->getMaterial(0).setTexture(0, texture);
	node->getMaterial(0).MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;

	return node;
}


bool SceneWorldRender::assignBlockTexture(const BlockTile tile, scene::ISceneNode *node)
{
	auto &mat = node->getMaterial(0);
	mat.ZWriteEnable = video::EZW_AUTO;
	// For EMT_TRANSPARENT_ALPHA_CHANNEL_REF : alpha threshold to clip
	mat.MaterialTypeParam = 0.5f;

	node->getMaterial(0).forEachTexture([](video::SMaterialLayer &layer) {
		layer.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
		layer.LODBias = -8; // slightly shaper edges
	});
	if (!tile.texture) {
		node->getMaterial(0).setTexture(0, g_blockmanager->getMissingTexture());
		return true;
	}

	switch (tile.type) {
		case BlockDrawType::Solid:
			if (tile.have_alpha)
				mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		case BlockDrawType::Action:
			mat.MaterialType = tile.have_alpha
				? video::EMT_TRANSPARENT_ALPHA_CHANNEL
				: video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		case BlockDrawType::Decoration:
			mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		default: break;
	}

	node->getMaterial(0).setTexture(0, tile.texture);
	return mat.MaterialType == video::EMT_SOLID;
}


void SceneWorldRender::updatePlayerPositions(float dtime)
{
	auto smgr = m_world_smgr;
	auto godmode_texture = m_gui->driver->getTexture("assets/textures/god_aura.png");

	// Smiley "units" of the full texture - needed for correct proportions.
	u32 smiley_texture_width;
	{
		const auto img_dim = m_gameplay->smiley_texture->getOriginalSize();
		smiley_texture_width = img_dim.Width / img_dim.Height;
	}

	do {
		if (m_nametag_force_show) {
			m_nametag_show_timer = 420; // show it
			break;
		}

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
	const auto players = m_gui->getClient()->getPlayerList();
	for (auto &p_it : *players.ptr()) {
		auto player = dynamic_cast<LocalPlayer *>(p_it.second.get());

		core::vector2di bp(player->pos.X + 0.5f, player->pos.Y + 0.5f);
		if (!m_drawn_rect.isPointInside(bp))
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
			// Smiley
			nf = smgr->addBillboardSceneNode(m_players_node,
				core::dimension2d<f32>(15, 15),
				nf_pos,
				nf_id
			);
			nf->forEachMaterial([](video::SMaterial &mat) {
				mat.ZWriteEnable = video::EZW_AUTO;
				mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			});
			nf->getMaterial(0).forEachTexture([](video::SMaterialLayer &layer) {
				layer.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
				layer.MagFilter = video::ETMAGF_LINEAR;
			});
			nf->getMaterial(0).setTexture(0, m_gameplay->smiley_texture);

			// Add nametag
			auto nt_texture = m_gameplay->generateTexture(player->name);
			auto nt_size = nt_texture->getOriginalSize();
			auto nt = smgr->addBillboardSceneNode(nf,
				core::dimension2d<f32>(nt_size.Width * 0.4f, nt_size.Height * 0.4f),
				core::vector3df(0, -10, -5),
				nf_id + 1
			);
			nt->forEachMaterial([](video::SMaterial &mat){
				mat.ZWriteEnable = video::EZW_AUTO;
				//mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			});
			nt->getMaterial(0).setTexture(0, nt_texture);
		}

		if (player->smiley_id < m_gameplay->smiley_count) {
			// Assign smiley texture offset
			auto &mat = nf->getMaterial(0).getTextureMatrix(0);
			mat.setTextureTranslate(player->smiley_id / (float)smiley_texture_width, 0);
			mat.setTextureScale(1.0f / smiley_texture_width, 1);
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
