#include "CParticlesSceneNode.h"

#include "particles.h"

CParticlesSceneNode::CParticlesSceneNode(ISceneNode *parent, scene::ISceneManager *mgr, s32 id,
	const core::vector3df &pos, const core::dimension2d<f32> tile_size, const Particles &p) :
	CBulkSceneNode(parent, mgr, id, pos, {0, 0}),
	m_particles(&p)
{
	m_tile_size = tile_size; // after CBulkSceneNode ctor
}

CParticlesSceneNode::~CParticlesSceneNode()
{
}

void CParticlesSceneNode::setBoundingBoxUnscaled(const core::aabbox3df &box)
{
	m_buffer->BoundingBox.MaxEdge = box.MaxEdge * m_tile_size.Width;
	m_buffer->BoundingBox.MinEdge = box.MinEdge * m_tile_size.Width;
}

void CParticlesSceneNode::OnAnimate(u32 t_ms)
{
	auto &vertices = m_buffer->Vertices->Data;
	auto &indices = m_buffer->Indices->Data;
	if (!vertices.empty())
		return;

	auto &positions = m_particles->pos;
	auto &sizes     = m_particles->size;
	vertices.resize(positions.size() * 4);
	indices.resize(positions.size() * 6);

	// TODO: Move this to a shader?
	for (size_t i = 0; i < positions.size(); ++i) {
		auto v_offset = &vertices[4 * i];
		auto i_offset = &indices[6 * i];

		i_offset[0] = 0 + (4 * i);
		i_offset[1] = 2 + (4 * i);
		i_offset[2] = 1 + (4 * i);
		i_offset[3] = 0 + (4 * i);
		i_offset[4] = 3 + (4 * i);
		i_offset[5] = 2 + (4 * i);

		for (unsigned j = 0; j < 4; ++j)
			v_offset[j].Color = 0xFFFFFFFF;

		v_offset[0].TCoords.set(1.0f, 1.0f);
		v_offset[1].TCoords.set(1.0f, 0.0f);
		v_offset[2].TCoords.set(0.0f, 0.0f);
		v_offset[3].TCoords.set(0.0f, 1.0f);
	}

	// TODO: Move this to a shader?
	// TODO: Update the bounding box
	const int grid_scale = m_tile_size.Width;
	auto pos_ptr = m_particles->pos.data();
	for (size_t i = 0; i < positions.size(); ++i) {
		const core::vector3df pos(
			pos_ptr[i].X * grid_scale,
			pos_ptr[i].Y * -grid_scale,
			0
		);
		const float s_2 = sizes[i % sizes.size()] * grid_scale / 2.0f;

		auto v_offset = &vertices[4 * i];

		/* Vertices are:
		2--1
		|\ |
		| \|
		3--0
		*/

		v_offset[0].Pos = pos + core::vector3df( s_2, -s_2, 0);
		v_offset[1].Pos = pos + core::vector3df( s_2,  s_2, 0);
		v_offset[2].Pos = pos + core::vector3df(-s_2,  s_2, 0);
		v_offset[3].Pos = pos + core::vector3df(-s_2, -s_2, 0);
	}

	m_buffer->setDirty();
}
