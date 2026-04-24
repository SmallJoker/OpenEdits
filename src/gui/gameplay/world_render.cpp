#include "world_render.h"
#include "gameplay.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "core/blockmanager.h"
#include "core/packet.h"
#include "core/smileymanager.h"
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
static float ZINDEX_OFFSET_MY_PLAYER = -1;

static float ZINDEX_LOOKUP[(int)BlockDrawType::Invalid + 1] = {
	2, // Solid
	2, // Action
	-2, // Decoration
	5, // Background
	0, // Invalid
};
static float ZINDEX_SHADOW = 3;

SceneWorldRender::SceneWorldRender(SceneGameplay *parent, Gui *gui)
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
	if (!m_world_smgr)
		m_world_smgr = m_gui->scenemgr->createNewSceneManager(false);
	m_world_smgr->clear();

	m_animation_timers.clear();

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

	updateAnimation(dtime);
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
		CBulkSceneNode *shadow_node = nullptr;
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

	// Prevent cute little deadlock with incoming packets: player first, world after.
	PtrLock<LocalPlayer> player = client->getMyPlayer();
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
		modified.LowerRightCorner += 1; // max pos inclusive

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

	// Remove soon-to-be dangling pointers
	for (auto &it : m_animation_timers)
		it.second.mat = nullptr;

	m_blocks_node->removeAll();


	// Draw more than necessary to skip on render steps when moving only slightly
	m_drawn_rect = core::recti(
		core::vector2di(x_center - x_extent - 2, y_center - y_extent - 2),
		core::vector2di(x_center + x_extent + 2, y_center + y_extent + 2)
	);
	m_drawn_rect.clipAgainst(world_border);
	const auto upperleft = m_drawn_rect.UpperLeftCorner; // move to stack
	const auto lowerright = m_drawn_rect.LowerRightCorner;

	BlockDrawData bdd;
	bdd.player = player.ptr();
	bdd.world = world.get();

	// This is very slow. Isn't there a faster way to draw stuff?
	// also camera->setFar(-camera_pos.Z + 0.1) does not filter them out (bug?)
	for (int y = upperleft.Y; y <= lowerright.Y; y++)
	for (int x = upperleft.X; x <= lowerright.X; x++) {
		bdd.pos = blockpos_t(x, y);
		if (!world->getBlock(bdd.pos, &bdd.b))
			continue;

		// Let's hope those two get "optimized away"
		Block &b = bdd.b;

		bdd.bulk = nullptr;

		do {
			// Unique ID for each appearance type
			size_t tile_hash = b.tile;

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

			if (b.id != 0) {
				size_t hash_node_id = BlockDrawData::hash(bdd.b.id, tile_hash);
				bdd.bulk = &bdd.bulk_map[hash_node_id];
				if (!bdd.bulk->node) {
					// Yet not cached: Add.
					assignNewForeground(bdd);
				}

				bdd.bulk->node->addTile({x, -y});
			}

			drawBlockParams(bdd);
		} while (false);

		{
			// Add background

			bdd.bulk = &bdd.bulk_map[b.bg];
			if (!bdd.bulk->node) {
				// Yet not cached: Add.
				assignNewBackground(bdd);
			}

			bdd.bulk->node->addTile({x, -y});
		}
	}

	for (auto &kv : bdd.bulk_map) {
		if (kv.second.shadow_node) {
			kv.second.shadow_node->copyTilesFrom(kv.second.node, 0x55000000);
		}
	}
}

static const core::dimension2d<f32> DEFAULT_TILE_SIZE(10, 10);
static const BlockTile FALLBACK_TILE;

void SceneWorldRender::assignNewForeground(BlockDrawData &bdd)
{
	auto smgr = m_gui->scenemgr;

	const BlockProperties *props = g_blockmanager->getProps(bdd.b.id);
	const BlockTile &tile = props ? props->getTileRef(bdd.b) : FALLBACK_TILE;
	auto z = ZINDEX_LOOKUP[(int)tile.type];

	// New scene node
	bdd.bulk->node = new CBulkSceneNode(m_blocks_node, smgr, -1,
		core::vector3df(0, 0, z),
		DEFAULT_TILE_SIZE
	);
	bdd.bulk->node->drop();
	bdd.bulk->is_solid = assignBlockTexture(tile, bdd.bulk->node);

	//bdd.bulk->node->setDebugDataVisible(scene::EDS_BBOX);

	{
		// Add shadow.
		auto node = new CBulkSceneNode(m_blocks_node, smgr, -1,
			core::vector3df(1.5f, -1.5f, ZINDEX_SHADOW),
			DEFAULT_TILE_SIZE
		);
		node->drop();

		node->getMaterial(0) = bdd.bulk->node->getMaterial(0);
		node->getMaterial(0).MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
		// ^ TODO: Shadows are always black despite vertex alpha. Why? Is this a draw order problem?

#if 0
		node->getMaterial(0).BlendOperation = video::EBO_ADD;
		node->getMaterial(0).BlendFactor = video::pack_textureBlendFuncSeparate(
			video::EBF_ONE, video::EBF_ONE_MINUS_SRC_COLOR,
			video::EBF_SRC_ALPHA, video::EBF_ONE,
			video::EMFN_MODULATE_1X, video::EAS_TEXTURE
		);
		ASSERT_FORCED(node->getMaterial(0).isAlphaBlendOperation(), "Required");
#endif

		bdd.bulk->shadow_node = node;
	}
}

void SceneWorldRender::assignNewBackground(BlockDrawData &bdd)
{
	auto smgr = m_gui->scenemgr;

	const BlockProperties *props = g_blockmanager->getProps(bdd.b.bg);
	const BlockTile &tile = props
		? props->tiles[0] // backgrounds cannot change (yet?)
		: FALLBACK_TILE;
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
	const Block *b = bdd.world->getBlockPtr(bdd.pos);
	const BlockProperties *props = g_blockmanager->getProps(b->id);

	if (props && props->overlay.type == TileOverlayType::Invalid)
		return;

	const TileCacheEntry entry = m_gui->getClient()->getTileCacheMgr().getOrCache(b);
	if (entry.overlay.empty())
		return;

	DEBUG_LOG("ADD OVERLAY @ %d,%d str=%s\n",
		bdd.pos.X, bdd.pos.Y, entry.overlay.c_str()
	);

	const size_t upper_hash = 0
		| (size_t)0xFF // any tile
		| std::hash<std::string>{}(entry.overlay) << 8;
	const size_t hash_node_id = BlockDrawData::hash(b->id, upper_hash);

	auto overlay = &bdd.bulk_map[hash_node_id];
	if (!overlay->node) {
		auto texture = m_gameplay->generateTexture(
			entry.overlay.c_str(),
			props->overlay.fg_color,
			props->overlay.bg_color
		);

		switch (props->overlay.type) {
		case TileOverlayType::Text_BottomRight:
			overlay->node = drawBottomLeftText(texture);
			break;
		case TileOverlayType::Text_FullSize:
			overlay->node = drawFullSizeText(texture);
			break;
		case TileOverlayType::Invalid:
			ASSERT_FORCED(false, "unreachable?");
		}
	}
	overlay->node->addTile({bdd.pos.X, -bdd.pos.Y});
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

CBulkSceneNode *SceneWorldRender::drawFullSizeText(video::ITexture *texture)
{
	auto dim_i = texture->getOriginalSize();
	core::dimension2df dim;
	dim.Height = 8;
	dim.Width = (float)dim_i.Width / dim_i.Height * dim.Height;

	// Align right
	auto node = new CBulkSceneNode(m_blocks_node, m_gui->scenemgr, -1,
		core::vector3df(0, (DEFAULT_TILE_SIZE.Height - dim.Height) / 2, 0.5),
		DEFAULT_TILE_SIZE
	);
	node->drop();

	node->setVertexSize(dim);
	node->getMaterial(0).setTexture(0, texture);
	node->getMaterial(0).MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;

	return node;
}

bool SceneWorldRender::assignBlockTexture(const BlockTile &tile, scene::ISceneNode *node)
{
	auto &mat = node->getMaterial(0);
	mat.ZWriteEnable = video::EZW_AUTO;
	// For EMT_TRANSPARENT_ALPHA_CHANNEL_REF : alpha threshold to clip
	mat.MaterialTypeParam = 0.5f;

	mat.forEachTexture([](video::SMaterialLayer &layer) {
		layer.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
		layer.LODBias = -8; // slightly shaper edges
	});

	size_t index = 0;
	if (tile.textures.size() > 1) {
		auto insertion = m_animation_timers.emplace(&tile, Animation());
		Animation &anim = insertion.first->second;
		anim.mat = &mat;
		index = anim.index;
	}

	video::ITexture *texture = tile.textures[index];
	if (!texture) {
		// Needed to render unknown blocks
		mat.setTexture(0, g_blockmanager->getMissingTexture());
		return true;
	}
	mat.setTexture(0, texture);

	switch (tile.type) {
		case BlockDrawType::Solid:
			if (tile.have_alpha)
				mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		case BlockDrawType::Action:
		case BlockDrawType::Decoration:
			mat.MaterialType = tile.have_alpha
				? video::EMT_TRANSPARENT_ALPHA_CHANNEL
				: video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			break;
		default: break;
	}

	return mat.MaterialType == video::EMT_SOLID;
}


void SceneWorldRender::updateAnimation(float dtime)
{
	for (auto &it : m_animation_timers) {
		Animation &anim = it.second;
		if (!anim.mat)
			continue;

		const BlockTile *tile = it.first;

		anim.timer += dtime;
		if (anim.timer < tile->animation_delay)
			continue;
		anim.timer -= tile->animation_delay;

		anim.index = (anim.index + 1) % it.first->textures.size();
		anim.mat->setTexture(0, it.first->textures[anim.index]);
	}
}


void SceneWorldRender::updatePlayerPositions(float dtime)
{
	Client *client = m_gui->getClient();

	auto smgr = m_world_smgr;
	auto tex_god_aura = m_gui->driver->getTexture("assets/textures/god_aura.png");
	auto tex_speech   = m_gui->driver->getTexture("assets/textures/speech_indicator.png");
	auto smileymgr = m_gui->getClient()->getSmileyMgr();

	do {
		if (m_nametag_force_show) {
			m_nametag_show_timer = 420; // show it
			break;
		}

		// Hide nametags after a certain duration
		// Nested because "getMyPlayer" contains a lock
		auto me = client->getMyPlayer();
		if (!me)
			break;

		if (me->vel.getLengthSQ() < 10 * 10)
			m_nametag_show_timer += dtime;
		else
			m_nametag_show_timer = 0;
	} while (0);

	enum : s32 {
		OFFSET_FACE = 0, // in m_players_node

		// Children of the player node. Use ascending IDs such
		// that deep node ID searches do not match them by accident.
		OFFSET_GOD_AURA,
		OFFSET_NAMETAG,
		OFFSET_SPEECH,
		// free space for other player decorations (effects?)
		OFFSET_MAX = 10
	};

	std::list<scene::ISceneNode *> children = m_players_node->getChildren();
	const auto players = client->getPlayerList();
	const peer_t my_peer_id = client->getMyPeerId();
	for (auto &p_it : *players.ptr()) {
		const auto player = dynamic_cast<LocalPlayer *>(p_it.second.get());

		core::vector2di bp(player->pos.X + 0.5f, player->pos.Y + 0.5f);
		if (!m_drawn_rect.isPointInside(bp))
			continue;

		// Draw the current player in front of all others
		const float offset = ZINDEX_OFFSET_MY_PLAYER * (player->peer_id == my_peer_id);
		core::vector3df nf_pos(
			player->pos.X * 10,
			player->pos.Y * -10,
			ZINDEX_SMILEY[player->godmode] + offset
		);

		if (player->node_id < 0) {
			player->node_id = m_player_node_id_counter;
			m_player_node_id_counter += OFFSET_MAX;
		}

		const s32 node_id = player->node_id;

		scene::ISceneNode *nf = nullptr;
		for (auto &c : children) {
			if (c && c->getID() == (node_id + OFFSET_FACE)) {
				nf = c;
				c = nullptr; // mark as handled
			}
		}

		auto smiley = smileymgr->getSmileyAt(player->smiley_id);

		if (nf) {
			nf->setPosition(nf_pos);
		} else {
			// Smiley
			nf = smgr->addBillboardSceneNode(m_players_node,
				core::dimension2d<f32>(15, 15),
				nf_pos,
				(node_id + OFFSET_FACE)
			);
			nf->forEachMaterial([](video::SMaterial &mat) {
				mat.ZWriteEnable = video::EZW_AUTO;
				mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			});
			nf->getMaterial(0).forEachTexture([](video::SMaterialLayer &layer) {
				layer.MinFilter = video::ETMINF_LINEAR_MIPMAP_LINEAR;
				layer.MagFilter = video::ETMAGF_LINEAR;
			});
			nf->getMaterial(0).setTexture(0, smiley.first->texture);

			// Add nametag
			auto nt_texture = m_gameplay->generateTexture(player->name);
			auto nt_size = nt_texture->getOriginalSize();
			auto nt = smgr->addBillboardSceneNode(nf,
				core::dimension2d<f32>(nt_size.Width * 0.4f, nt_size.Height * 0.4f),
				core::vector3df(0, -10, -5),
				(node_id + OFFSET_NAMETAG)
			);
			nt->forEachMaterial([](video::SMaterial &mat){
				mat.ZWriteEnable = video::EZW_AUTO;
				//mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;
			});
			nt->getMaterial(0).setTexture(0, nt_texture);
		}

		if (smiley.first->texture) {
			float width = smiley.first->texture_width;
			// Assign smiley texture offset
			auto &mat = nf->getMaterial(0).getTextureMatrix(0);
			mat.setTextureTranslate(smiley.second / width, 0.0f);
			mat.setTextureScale(1.0f / width, 1.0f);
		}

		const bool nametags_visible = m_nametag_show_timer > 1.0;
		scene::ISceneNode *ga = nullptr,
			*ci = nullptr;
		for (auto &c : nf->getChildren()) {
			const s32 offset = c->getID() - node_id;
			switch (offset) {
				case OFFSET_GOD_AURA:
					ga = c;
					break;
				case OFFSET_NAMETAG:
					c->setVisible(nametags_visible);
					break;
				case OFFSET_SPEECH:
					ci = c;
					break;
			}
		}

		// Update god aura if needed
		if (player->godmode == (!!ga)) {
			// OK, no change needed.
		} else if (player->godmode) {
			auto node = smgr->addBillboardSceneNode(nf,
				core::dimension2d<f32>(18, 18),
				core::vector3df(0, 0, 0.1),
				(node_id + OFFSET_GOD_AURA)
			);

			node->forEachMaterial([](video::SMaterial &mat){
				mat.ZWriteEnable = video::EZW_AUTO;
				mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			});
			node->getMaterial(0).setTexture(0, tex_god_aura);
		} else {
			ga->remove();
		}

		const bool speech_visible = player->speech_countdown > 0.0f;

		if (speech_visible == (!!ci)) {
			// OK, no change needed.
		} else if (speech_visible) {
			auto node = smgr->addBillboardSceneNode(nf,
				core::dimension2d<f32>(7, 7),
				core::vector3df(8, 6, -0.1),
				(node_id + OFFSET_SPEECH)
			);

			node->forEachMaterial([](video::SMaterial &mat){
				mat.ZWriteEnable = video::EZW_AUTO;
				mat.MaterialType = video::EMT_TRANSPARENT_ALPHA_CHANNEL;
			});
			node->getMaterial(0).setTexture(0, tex_speech);
		} else {
			// TODO: A fade animation (shrink or alpha) would be nice.
			ci->remove();
		}
	}

	// Remove any remaining entries
	for (auto c : children) {
		if (c)
			m_players_node->removeChild(c);
	}

	//printf("drawing %zu players\n", m_players->getChildren().size());
}
