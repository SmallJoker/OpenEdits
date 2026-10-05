#pragma once

#include "CBulkSceneNode.h"

struct Particles;

class CParticlesSceneNode : public CBulkSceneNode {
public:
	CParticlesSceneNode(ISceneNode *parent, scene::ISceneManager *mgr, s32 id,
		const core::vector3df &pos, const core::dimension2d<f32> tile_size,
		const Particles &p);

	~CParticlesSceneNode();

	void setBoundingBoxUnscaled(const core::aabbox3df &box);

	void OnAnimate(u32 t_ms) override;

private:
	const Particles *m_particles;
};
