#include "CBulkSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "ICameraSceneNode.h"

CBulkSceneNode::CBulkSceneNode(ISceneNode *parent, scene::ISceneManager *mgr, s32 id,
	const core::vector3df &pos, const core::dimension2d<f32> &tile_size) :
	scene::ISceneNode(parent, mgr, id, pos),
	m_buffer(new scene::SMeshBuffer())
{
	m_tile_size = tile_size;
	m_vertex_size = tile_size;
	m_tiles.reserve(20 * 20);

	m_buffer->setHardwareMappingHint(scene::EHM_STATIC);
}

CBulkSceneNode::~CBulkSceneNode()
{
	m_buffer->drop();
}

void CBulkSceneNode::addTile(core::vector2di coord)
{
	m_tiles.push_back(coord);

	/*
		X+ = right side
		Y+ = bottom side
	*/
	const f32 x = m_tile_size.Width * (coord.X - 0.5f);
	const f32 y = m_tile_size.Height * (coord.Y - 0.5f);

	if (m_tiles.size() == 1) {
		m_bbox_large.reset(x, y, RelativeTranslation.Z);
	} else {
		m_bbox_large.addInternalPoint(x, y, RelativeTranslation.Z + 1);
	}
	m_bbox_large.addInternalPoint(x + m_vertex_size.Width, y + m_vertex_size.Height, RelativeTranslation.Z + 1);
}

video::SMaterial &CBulkSceneNode::getMaterial(u32 i)
{
	return m_buffer->Material;
}

const core::aabbox3d<f32> &CBulkSceneNode::getBoundingBox() const
{
	return m_bbox_large;
}

void CBulkSceneNode::OnRegisterSceneNode()
{
	if (IsVisible)
		SceneManager->registerNodeForRendering(this);

	ISceneNode::OnRegisterSceneNode();
}

void CBulkSceneNode::OnAnimate(u32 t_ms)
{
	auto &vertices = m_buffer->Vertices;
	auto &indices = m_buffer->Indices;
	const size_t vertices_size_old = vertices.size();
	const size_t vertices_size_new = 4 * m_tiles.size();

	vertices.set_used(vertices_size_new);
	indices.set_used(6 * m_tiles.size());

	if (vertices_size_new <= vertices_size_old)
		return; // Nothing to update

	// Newly allocated data -> initialize with geometry information
	for (size_t i = vertices_size_old / 4; i < m_tiles.size(); ++i) {
		auto v_offset = &vertices[4 * i];
		auto i_offset = &indices[6 * i];

		i_offset[0] = 0 + (4 * i);
		i_offset[1] = 2 + (4 * i);
		i_offset[2] = 1 + (4 * i);
		i_offset[3] = 0 + (4 * i);
		i_offset[4] = 3 + (4 * i);
		i_offset[5] = 2 + (4 * i);

		for (s32 j = 0; j < 4; ++j)
			v_offset[j].Color = 0xFFFFFFFF;

		v_offset[0].TCoords.set(1.0f, 1.0f);
		v_offset[1].TCoords.set(1.0f, 0.0f);
		v_offset[2].TCoords.set(0.0f, 0.0f);
		v_offset[3].TCoords.set(0.0f, 1.0f);
	}

	const f32 TILE_W = m_tile_size.Width;
	const f32 TILE_H = m_tile_size.Height;

	// See also: size prediction in CBulkSceneNode::addTile(...)
	core::vector3df node_pos = getAbsolutePosition()
		- core::vector3df(TILE_W, TILE_H, 0) / 2; // center

	core::vector3df h_len(m_vertex_size.Width, 0, 0);
	core::vector3df v_len(0, m_vertex_size.Height, 0);
	for (size_t i = vertices_size_old / 4; i < m_tiles.size(); ++i) {
		auto v_offset = &vertices[4 * i];

		/* Vertices are:
		2--1
		|\ |
		| \|
		3--0
		*/
		auto pos = node_pos;
		pos.X += m_tiles[i].X * TILE_W;
		pos.Y += m_tiles[i].Y * TILE_H;
		v_offset[0].Pos = pos + h_len;
		v_offset[1].Pos = pos + h_len + v_len;
		v_offset[2].Pos = pos + v_len;
		v_offset[3].Pos = pos;
	}

	m_buffer->setDirty();
	//m_buffer->recalculateBoundingBox();
	m_buffer->setBoundingBox(m_bbox_large);
}


// Based on CBillboardSceneNode::render()
void CBulkSceneNode::render()
{
	auto driver = SceneManager->getVideoDriver();
	auto camera = SceneManager->getActiveCamera();

	if (!camera || !driver)
		return;

	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->setMaterial(m_buffer->Material);
	driver->drawMeshBuffer(m_buffer);

	if (DebugDataVisible & scene::EDS_BBOX)
	{
		driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
		video::SMaterial m;
		m.Lighting = false;
		driver->setMaterial(m);
		driver->draw3DBox(m_bbox_large, video::SColor(0,208,195,152));
	}
}
