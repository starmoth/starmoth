// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _HUDTRAIL_H
#define _HUDTRAIL_H

// trail drawn after an object to track motion

#include "libs.h"
#include "Body.h"
#include "graphics/Renderer.h"

class HudTrail
{
public:
	HudTrail(Body *b, const Color&);
	void Update(float time);
	void Render(Graphics::Renderer *r);
	void Reset(const Frame *newFrame);

	void SetColor(const Color &c) { m_color = c; }
	void SetTransform(const matrix4x4d &t) { m_transform = t; }

private:
	void RefreshVertexBuffer(Graphics::Renderer *r, const Uint32 size);

	Body *m_body;
	const Frame *m_currentFrame;
	float m_updateTime;
	Color m_color;
	matrix4x4d m_transform;
	std::deque<vector3d> m_trailPoints;
	Graphics::RenderState *m_renderState;
	bool m_refreshVB;
	RefCountedPtr<Graphics::Material> m_material;
	std::unique_ptr<Graphics::VertexBuffer> m_vbuffer;
};

#endif
