// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Label3D.h"
#include "NodeVisitor.h"
#include "graphics/Renderer.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"

namespace SceneGraph {

#pragma pack(push, 4)
struct Label3DVert {
	vector3f pos;
	vector3f norm;
	vector2f uv;
};
#pragma pack(pop)

Label3D::Label3D(Graphics::Renderer *r, RefCountedPtr<Text::DistanceFieldFont> font)
: Node(r, NODE_TRANSPARENT)
, m_font(font)
{
	Graphics::MaterialDescriptor matdesc;
	matdesc.textures = 1;
	matdesc.alphaTest = true;
	matdesc.lighting = true;
	m_geometry.reset(font->CreateVertexArray());
	m_material.Reset(r->CreateMaterial(matdesc));
	m_material->texture0 = font->GetTexture();
	m_material->diffuse = Color::WHITE;
	m_material->emissive = Color(38);
	m_material->specular = Color::WHITE;

	Graphics::RenderStateDesc rsd;
	rsd.depthWrite = false;
	m_renderState = r->CreateRenderState(rsd);
}

Label3D::Label3D(const Label3D &label, NodeCopyCache *cache)
: Node(label, cache)
, m_material(label.m_material)
, m_font(label.m_font)
, m_renderState(label.m_renderState)
{
	m_geometry.reset(m_font->CreateVertexArray());
}

Node* Label3D::Clone(NodeCopyCache *cache)
{
	return new Label3D(*this, cache);
}

void Label3D::SetText(const std::string &text)
{
	//regenerate geometry
	m_geometry->Clear();
	if (!text.empty()) {
		m_font->GetGeometry(*m_geometry, text, vector2f(0.f));
		
		//create buffer and upload data
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.attrib[1].semantic = Graphics::ATTRIB_NORMAL;
		vbd.attrib[1].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.attrib[2].semantic = Graphics::ATTRIB_UV0;
		vbd.attrib[2].format   = Graphics::ATTRIB_FORMAT_FLOAT2;
		vbd.numVertices = m_geometry->GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;
		m_material->SetupVertexBufferDesc( vbd );

		m_vbuffer.reset( m_renderer->CreateVertexBuffer(vbd) );
		Label3DVert* vtxPtr = m_vbuffer->Map<Label3DVert>(Graphics::BUFFER_MAP_WRITE);
		assert(m_vbuffer->GetDesc().stride == sizeof(Label3DVert));
		for(Uint32 i=0 ; i<vbd.numVertices ; i++)
		{
			vtxPtr[i].pos	= m_geometry->position[i];
			vtxPtr[i].norm	= m_geometry->normal[i];
			vtxPtr[i].uv	= m_geometry->uv0[i];
		}
		m_vbuffer->Unmap();
	}
}

void Label3D::Render(const matrix4x4f &trans, const RenderData *rd)
{
	Graphics::Renderer *r = GetRenderer();
	r->SetTransform(trans);
	r->DrawBuffer(m_vbuffer.get(), m_renderState, m_material.Get());
}

void Label3D::Accept(NodeVisitor &nv)
{
	nv.ApplyLabel(*this);
}

}
