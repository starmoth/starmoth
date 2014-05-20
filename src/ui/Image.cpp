// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Image.h"
#include "Context.h"
#include "graphics/TextureBuilder.h"
#include "graphics/VertexArray.h"

namespace UI {

#pragma pack(push, 4)
struct ImageVert {
	vector3f pos;
	vector2f uv;
};
#pragma pack(pop)

Image::Image(Context *context, const std::string &filename, Uint32 sizeControlFlags): Widget(context)
{
	Graphics::Renderer *r = GetContext()->GetRenderer();
	Graphics::TextureBuilder b = Graphics::TextureBuilder::UI(filename);
	m_texture.Reset(b.GetOrCreateTexture(r, "ui"));

	const Graphics::TextureDescriptor &descriptor = b.GetDescriptor();
	m_initialSize = Point(descriptor.dataSize.x*descriptor.texSize.x,descriptor.dataSize.y*descriptor.texSize.y);

	Graphics::MaterialDescriptor material_desc;
	material_desc.textures = 1;
	m_material.Reset(r->CreateMaterial(material_desc));
	m_material->texture0 = m_texture.Get();
	
	const vector2f texSize = m_texture->GetDescriptor().texSize;
	Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_UV0);
	va.Add(vector3f(0.0f, 0.0f, 0.0f), vector2f(0.0f,      0.0f));
	va.Add(vector3f(0.0f, 1.0f, 0.0f), vector2f(0.0f,      texSize.y));
	va.Add(vector3f(1.0f, 0.0f, 0.0f), vector2f(texSize.x, 0.0f));
	va.Add(vector3f(1.0f, 1.0f, 0.0f), vector2f(texSize.x, texSize.y));

	m_vbuffer;
	//create buffer and upload data
	Graphics::VertexBufferDesc vbd;
	vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
	vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[1].semantic = Graphics::ATTRIB_UV0;
	vbd.attrib[1].format   = Graphics::ATTRIB_FORMAT_FLOAT2;
	vbd.numVertices = va.GetNumVerts();
	vbd.usage = Graphics::BUFFER_USAGE_STATIC;
	m_material->SetupVertexBufferDesc( vbd );

	m_vbuffer.Reset( r->CreateVertexBuffer(vbd) );
	ImageVert* vtxPtr = m_vbuffer->Map<ImageVert>(Graphics::BUFFER_MAP_WRITE);
	assert(m_vbuffer->GetDesc().stride == sizeof(ImageVert));
	for(Uint32 i=0 ; i<vbd.numVertices ; i++)
	{
		vtxPtr[i].pos	= va.position[i];
		vtxPtr[i].uv	= va.uv0[i];
	}
	m_vbuffer->Unmap();

	SetSizeControlFlags(sizeControlFlags);
}

Point Image::PreferredSize()
{
	return m_initialSize;
}

Image *Image::SetHeightLines(Uint32 lines)
{
	const Text::TextureFont *font = GetContext()->GetFont(GetFont()).Get();
	const float height = font->GetHeight() * lines;
	m_initialSize = UI::Point(height * float(m_initialSize.x) / float(m_initialSize.y), height);
	GetContext()->RequestLayout();
	return this;
}

void Image::Draw()
{
	const Point &offset = GetActiveOffset();
	const Point &area = GetActiveArea();

	const float x = offset.x;
	const float y = offset.y;
	const float sx = area.x;
	const float sy = area.y;

	Graphics::Renderer *r = GetContext()->GetRenderer();

	Graphics::Renderer::MatrixTicket mt(r, Graphics::MatrixMode::MODELVIEW);

	matrix4x4f local(r->GetCurrentModelView());
	local.Translate(offset.x, offset.y, 0.0f);
	local.Scale(area.x, area.y, 0.0f);
	r->SetTransform(local);
	
	auto renderState = GetContext()->GetSkin().GetAlphaBlendState();
	m_material->diffuse = Color(Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, GetContext()->GetOpacity()*Color::WHITE.a);
	r->DrawBuffer(m_vbuffer.Get(), renderState, m_material.Get(), Graphics::TRIANGLE_STRIP);
}

}
