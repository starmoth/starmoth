// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "HudTrail.h"
#include "Pi.h"
#include "graphics/RenderState.h"

const float UPDATE_INTERVAL = 0.1f;
const Uint16 MAX_POINTS = 100;

HudTrail::HudTrail(Body *b, const Color& c)
: m_body(b)
, m_updateTime(0.f)
, m_color(c)
, m_refreshVB(true)
{
	m_currentFrame = b->GetFrame();

	Graphics::RenderStateDesc rsd;
	rsd.blendMode = Graphics::BLEND_ALPHA_ONE;
	rsd.depthWrite = false;
	m_renderState = Pi::renderer->CreateRenderState(rsd);
}

void HudTrail::Update(float time)
{
	//record position
	m_updateTime += time;
	if (m_updateTime > UPDATE_INTERVAL) {
		m_updateTime = 0.f;
		const Frame *bodyFrame = m_body->GetFrame();
		if( bodyFrame==m_currentFrame ) {
			m_trailPoints.push_back(m_body->GetInterpPosition());
			m_refreshVB = true;
		}
	}

	while (m_trailPoints.size() > MAX_POINTS) {
		m_trailPoints.pop_front();
		m_refreshVB = true;
	}
}

void HudTrail::Render(Graphics::Renderer *r)
{
	//render trail
	if (!m_trailPoints.empty()) {
		const vector3d vpos = m_transform * m_body->GetInterpPosition();
		m_transform[12] = vpos.x;
		m_transform[13] = vpos.y;
		m_transform[14] = vpos.z;
		m_transform[15] = 1.0;

		Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE);
		const vector3d curpos = m_body->GetInterpPosition();
		float alpha = 1.f;
		const float decrement = 1.f / m_trailPoints.size();
		va.Add(vector3f(0.f), Color(0.f));
		for (Uint16 i = m_trailPoints.size()-1; i > 0; i--) {
			alpha -= decrement;
			va.Add(-vector3f(curpos - m_trailPoints[i]), Color(m_color.r, m_color.g, m_color.b, alpha * 255));
		}

		if(m_refreshVB) {
			m_refreshVB = false;
			RefreshVertexBuffer( r, va.GetNumVerts() );
		}

		m_vbuffer->Populate( va );

		r->SetTransform(m_transform);
		r->DrawBuffer(m_vbuffer.get(), m_renderState, m_material.Get(), Graphics::LINE_STRIP);
	}
}

void HudTrail::Reset(const Frame *newFrame)
{
	m_currentFrame = newFrame;
	m_trailPoints.clear();
	m_refreshVB = true;
}

void HudTrail::RefreshVertexBuffer(Graphics::Renderer *r, const Uint32 size)
{
	Graphics::MaterialDescriptor desc;
	desc.vertexColors = true;
	m_material.Reset(r->CreateMaterial(desc));

	Graphics::VertexBufferDesc vbd;
	vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
	vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
	vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_UBYTE4;
	vbd.usage = Graphics::BUFFER_USAGE_DYNAMIC;
	vbd.numVertices = size;
	m_material->SetupVertexBufferDesc( vbd );
	m_vbuffer.reset(r->CreateVertexBuffer(vbd));
}
