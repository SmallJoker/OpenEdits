#pragma once

#include <IBillboardSceneNode.h>
#include <SMeshBuffer.h>
#include <vector>

using namespace irr;

class CBulkSceneNode : virtual public scene::ISceneNode {
public:
	CBulkSceneNode(ISceneNode *parent, scene::ISceneManager *mgr, s32 id,
		const core::vector3df &pos, const core::dimension2d<f32> &tile_size);

	~CBulkSceneNode();

	video::SMaterial &getMaterial(u32 i) override;
	u32 getMaterialCount() const override { return 1; };

	const core::aabbox3d<f32> &getBoundingBox() const override;

	void addTile(core::vector2di coord);

	void OnRegisterSceneNode() override;
	void render() override;

private:
	void updateMesh(const scene::ICameraSceneNode *camera);

	core::aabbox3d<f32> m_bbox_large;
	std::vector<core::vector2di> m_tiles;
	core::dimension2d<f32> m_tile_size;
	scene::SMeshBuffer *m_buffer;
};
