#pragma once

#include <IBillboardSceneNode.h>
#include <SMeshBuffer.h>
#include <vector>

using namespace irr;

class CBulkSceneNode : public scene::ISceneNode {
public:
	CBulkSceneNode(ISceneNode *parent, scene::ISceneManager *mgr, s32 id,
		const core::vector3df &pos, const core::dimension2d<f32> &tile_size);

	~CBulkSceneNode();

	video::SMaterial &getMaterial(u32 i) override;
	u32 getMaterialCount() const override { return 1; };

	/// Must be called before any addTile(...)
	void setVertexSize(core::dimension2d<f32> size) { m_vertex_size = size; }

	const core::aabbox3d<f32> &getBoundingBox() const override;

	void addTile(core::vector2di coord);

	void OnRegisterSceneNode() override;
	void OnAnimate(u32 t_ms) override;
	void render() override;

private:
	core::aabbox3d<f32> m_bbox_large;
	std::vector<core::vector2di> m_tiles;
	core::dimension2d<f32> m_tile_size, // uniform grid to place vertices
		m_vertex_size; // size of the vertex (may overlap tile size)
	scene::SMeshBuffer *m_buffer;
};
