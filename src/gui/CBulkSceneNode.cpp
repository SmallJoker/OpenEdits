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
	m_tiles.reserve(50);

	m_buffer->Vertices.set_used(8);
}

CBulkSceneNode::~CBulkSceneNode()
{
	m_buffer->drop();
}

void CBulkSceneNode::addTile(core::vector2di coord)
{
	m_tiles.push_back(coord);

	const f32 x = m_tile_size.Width * coord.X;
	const f32 y = m_tile_size.Height * coord.Y;

	m_bbox_large.addInternalPoint(x, y, RelativeTranslation.Z + 1);
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

// Based on CBillboardSceneNode::render()
void CBulkSceneNode::render()
{
	auto driver = SceneManager->getVideoDriver();
	auto camera = SceneManager->getActiveCamera();

	if (!camera || !driver)
		return;

	// make billboard look to camera
	updateMesh(camera);

	driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
	driver->setMaterial(m_buffer->Material);
	driver->drawMeshBuffer(m_buffer);

	if (DebugDataVisible & scene::EDS_BBOX || true)
	{
		driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
		video::SMaterial m;
		m.Lighting = false;
		driver->setMaterial(m);
		driver->draw3DBox(m_bbox_large, video::SColor(0,208,195,152));
	}
}

// Based on CBillboardSceneNode::updateMesh(...)
void CBulkSceneNode::updateMesh(const scene::ICameraSceneNode *camera)
{
	core::vector3df node_pos = getAbsolutePosition();

	core::vector3df campos = camera->getAbsolutePosition();
	core::vector3df target = camera->getTarget();
	core::vector3df view = target - campos;
	view.normalize();

	view *= -1.0f;

	auto &vertices = m_buffer->Vertices;
	auto &indices = m_buffer->Indices;
	const size_t vertices_size_old = vertices.size();

	vertices.set_used(4 * m_tiles.size());
	indices.set_used(6 * m_tiles.size());

	if (vertices.size() > vertices_size_old) {
		// Newly allocated data -> initialize with geometry information
		for (size_t i = vertices_size_old / 4; i < m_tiles.size(); ++i) {
			auto v_offset = &vertices[4 * i];
			auto i_offset = &indices[6 * i];

			i_offset[0] = 0;
			i_offset[1] = 2;
			i_offset[2] = 1;
			i_offset[3] = 0;
			i_offset[4] = 3;
			i_offset[5] = 2;

			for (s32 j = 0; j < 4; ++j)
				v_offset[j].Color = 0xFFFFFFFF;

			v_offset[0].TCoords.set(1.0f, 1.0f);
			v_offset[1].TCoords.set(1.0f, 0.0f);
			v_offset[2].TCoords.set(0.0f, 0.0f);
			v_offset[3].TCoords.set(0.0f, 1.0f);
		}

	}

	core::vector3df h_len(m_tile_size.Width, 0, 0);
	core::vector3df v_len(0, m_tile_size.Height, 0);
	for (size_t i = 0; i < m_tiles.size(); ++i) {
		auto v_offset = &vertices[4 * i];

		for (s32 j = 0; j < 4; ++j)
			v_offset[j].Normal = view;

		/* Vertices are:
		2--1
		|\ |
		| \|
		3--0
		*/
		auto pos = node_pos;
		pos.X += m_tiles[i].X * m_tile_size.Width;
		pos.Y += m_tiles[i].Y * m_tile_size.Height;
		v_offset[0].Pos = pos + h_len + v_len;
		v_offset[1].Pos = pos + h_len;
		v_offset[2].Pos = pos;
		v_offset[3].Pos = pos + v_len;
		//printf("@ %i , %i , %i\n", (int)pos.X, (int)pos.Y, (int)pos.Z);
	}

	m_buffer->setDirty(scene::EBT_VERTEX);
	m_buffer->recalculateBoundingBox();
}
