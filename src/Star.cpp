// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Star.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "gui/Gui.h"
#include "Pi.h"
#include <SDL_stdinc.h>

using namespace Graphics;

Star::Star() : TerrainBody()
{
}

Star::Star(SystemBody *sbody): TerrainBody(sbody)
{
	InitStar();
}

void Star::Load(Serializer::Reader &rd, Space *space)
{
	TerrainBody::Load(rd, space);		// to get sbody
	InitStar();
}

void Star::InitStar()
{
	// this one should be atmosphere radius when stars have atmosphere
	SetPhysRadius(GetMaxFeatureRadius());

	// this one is much larger because stars have halo effect
	// if star is wolf-rayet it gets a very large halo effect
	const SystemBody *sbody = GetSystemBody();
	const float wf = (sbody->GetType() < SystemBody::TYPE_STAR_S_BH && sbody->GetType() > SystemBody::TYPE_STAR_O_HYPER_GIANT) ? 100.0f : 1.0f;
	SetClipRadius(sbody->GetRadius() * 8 * wf);

	Graphics::RenderStateDesc rsd;
	rsd.blendMode = BLEND_ALPHA;
	rsd.depthWrite = false;
	m_haloState = Pi::renderer->CreateRenderState(rsd);
}

void Star::Render(Graphics::Renderer *renderer, const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform)
{
	double radius = GetClipRadius();

	double rad = radius;
	vector3d fpos = viewCoords;
	double len = fpos.Length();

	while (len > 1000.0f) {
		rad *= 0.25;
		fpos = 0.25*fpos;
		len *= 0.25;
	}

	matrix4x4d trans = matrix4x4d::Identity();
	trans.Translate(float(fpos.x), float(fpos.y), float(fpos.z));

	// face the camera dammit
	vector3d zaxis = viewCoords.NormalizedSafe();
	vector3d xaxis = vector3d(0,1,0).Cross(zaxis).Normalized();
	vector3d yaxis = zaxis.Cross(xaxis);
	matrix4x4d rot = matrix4x4d::MakeRotMatrix(xaxis, yaxis, zaxis).Inverse();

	renderer->SetTransform(trans * rot);

	const Uint8 *col = StarSystem::starRealColors[GetSystemBody()->GetType()];

	Random rand;

	//render star halo
	VertexArray va(ATTRIB_POSITION | ATTRIB_DIFFUSE);
	const Color bright(col[0], col[1], col[2], 255);
	const Color dark(0);

	va.Add(vector3f(0.f), bright);
	for (float ang=0; ang<2*M_PI; ang+=0.26183+rand.Double(0,0.4)) {
		va.Add(vector3f(rad*sin(ang), rad*cos(ang), 0), dark);
	}
	va.Add(vector3f(0.f, rad, 0.f), dark);

	if( !m_vbuffer.Valid() || m_vbuffer->GetVertexCount() != va.GetNumVerts() )
	{
		//create buffer and upload data
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
		vbd.attrib[1].format   = Graphics::ATTRIB_FORMAT_UBYTE4;
		vbd.numVertices = va.GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_DYNAMIC;	// we could be updating this per-frame
		Graphics::vtxColorMaterial->SetupVertexBufferDesc( vbd );
		m_vbuffer.Reset( renderer->CreateVertexBuffer(vbd) );
	}
	m_vbuffer->Populate( va );

	renderer->DrawBuffer(m_vbuffer.Get(), m_haloState, Graphics::vtxColorMaterial, TRIANGLE_FAN);

	TerrainBody::Render(renderer, camera, viewCoords, viewTransform);
}
