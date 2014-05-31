// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Skin.h"
#include "IniConfig.h"
#include "graphics/TextureBuilder.h"
#include "graphics/VertexArray.h"
#include "FileSystem.h"
#include "utils.h"

namespace UI {

static const float SKIN_SIZE = 512.0f;

Skin::Skin(const std::string &filename, Graphics::Renderer *renderer, float scale) :
	m_renderer(renderer),
	m_scale(scale),
	m_opacity(1.0f)
{
	IniConfig cfg;
	// set defaults
	cfg.SetInt("ButtonMinInnerSize", 16);
	cfg.SetFloat("AlphaNormal", 0.0);
	cfg.SetFloat("AlphaHover", 0.4);
	cfg.SetFloat("AlphaSelect", 0.6);
	// load
	cfg.Read(FileSystem::gameDataFiles, filename);

	m_texture.Reset(Graphics::TextureBuilder::UI(cfg.String("TextureFile")).GetOrCreateTexture(m_renderer, "ui"));

	Graphics::MaterialDescriptor desc;
	desc.textures = 1;
	m_textureMaterial.Reset(m_renderer->CreateMaterial(desc));
	m_textureMaterial->texture0 = m_texture.Get();
	m_textureMaterial->diffuse = Color::WHITE;

	desc.textures = 0;
	m_colorMaterial.Reset(m_renderer->CreateMaterial(desc));

	Graphics::RenderStateDesc rsd;
	rsd.blendMode = Graphics::BLEND_ALPHA;
	rsd.depthWrite = false;
	rsd.depthTest = false;
	m_alphaBlendState = m_renderer->CreateRenderState(rsd);

	rsd.blendMode = Graphics::BLEND_SET_ALPHA;
	m_alphaSetState = m_renderer->CreateRenderState(rsd);

	rsd.blendMode = Graphics::BLEND_DEST_ALPHA;
	m_alphaMaskState = m_renderer->CreateRenderState(rsd);

	m_backgroundNormal        = LoadBorderedRectElement(cfg.String("BackgroundNormal"));
	m_backgroundActive        = LoadBorderedRectElement(cfg.String("BackgroundActive"));

	m_buttonDisabled          = LoadBorderedRectElement(cfg.String("ButtonDisabled"));
	m_buttonNormal            = LoadBorderedRectElement(cfg.String("ButtonNormal"));
	m_buttonHover             = LoadBorderedRectElement(cfg.String("ButtonHover"));
	m_buttonActive            = LoadBorderedRectElement(cfg.String("ButtonActive"));

	m_smallButtonDisabled     = LoadRectElement(cfg.String("SmallButtonDisabled"));
	m_smallButtonNormal       = LoadRectElement(cfg.String("SmallButtonNormal"));
	m_smallButtonHover        = LoadRectElement(cfg.String("SmallButtonHover"));
	m_smallButtonActive       = LoadRectElement(cfg.String("SmallButtonActive"));

	m_checkboxDisabled        = LoadRectElement(cfg.String("CheckboxDisabled"));
	m_checkboxNormal          = LoadRectElement(cfg.String("CheckboxNormal"));
	m_checkboxHover           = LoadRectElement(cfg.String("CheckboxHover"));
	m_checkboxActive          = LoadRectElement(cfg.String("CheckboxActive"));
	m_checkboxCheckedDisabled = LoadRectElement(cfg.String("CheckboxCheckedDisabled"));
	m_checkboxCheckedNormal   = LoadRectElement(cfg.String("CheckboxCheckedNormal"));
	m_checkboxCheckedHover    = LoadRectElement(cfg.String("CheckboxCheckedHover"));
	m_checkboxCheckedActive   = LoadRectElement(cfg.String("CheckboxCheckedActive"));

	m_sliderVerticalGutter         = LoadEdgedRectElement(cfg.String("SliderVerticalGutter"), EdgedOrientation::VERTICAL);
	m_sliderHorizontalGutter       = LoadEdgedRectElement(cfg.String("SliderHorizontalGutter"), EdgedOrientation::HORIZONTAL);
	m_sliderVerticalButtonNormal   = LoadRectElement(cfg.String("SliderVerticalButtonNormal"));
	m_sliderVerticalButtonHover    = LoadRectElement(cfg.String("SliderVerticalButtonHover"));
	m_sliderVerticalButtonActive   = LoadRectElement(cfg.String("SliderVerticalButtonActive"));
	m_sliderHorizontalButtonNormal = LoadRectElement(cfg.String("SliderHorizontalButtonNormal"));
	m_sliderHorizontalButtonHover  = LoadRectElement(cfg.String("SliderHorizontalButtonHover"));
	m_sliderHorizontalButtonActive = LoadRectElement(cfg.String("SliderHorizontalButtonActive"));

	m_gaugeBackground = LoadEdgedRectElement(cfg.String("GaugeBackground"), EdgedOrientation::HORIZONTAL);
	m_gaugeMask = LoadEdgedRectElement(cfg.String("GaugeMask"), EdgedOrientation::HORIZONTAL);
	m_gaugeFillNormal = LoadRectElement(cfg.String("GaugeFillNormal"));
	m_gaugeFillWarning = LoadRectElement(cfg.String("GaugeFillWarning"));
	m_gaugeFillCritical = LoadRectElement(cfg.String("GaugeFillCritical"));

	m_buttonMinInnerSize      = cfg.Int("ButtonMinInnerSize");

	m_alphaNormal = cfg.Float("AlphaNormal");
	m_alphaSelect = cfg.Float("AlphaSelect");
	m_alphaHover  = cfg.Float("AlphaHover");
}

Graphics::RenderState *Skin::GetRenderState(Graphics::BlendMode mode) const
{
	if (mode == Graphics::BLEND_SET_ALPHA)
		return m_alphaSetState;
	else if (mode == Graphics::BLEND_DEST_ALPHA)
		return m_alphaMaskState;
	else
		return m_alphaBlendState;
}

static inline vector2f scaled(const vector2f &v)
{
	return v * (1.0f / SKIN_SIZE);
}

#pragma pack(push, 4)
struct SkinVert {
	vector3f pos;
	vector2f uv;
};

struct SkinPosOnlyVert {
	vector3f pos;
};
#pragma pack(pop)

void Skin::RectElement::GenerateVertexBuffer(Graphics::Renderer* renderer, Graphics::Material* material)
{
	Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_UV0);
	
	va.Add(vector3f(0.0f, 0.0f, 0.0f), scaled(vector2f(pos.x,        pos.y)));
	va.Add(vector3f(0.0f, 1.0f, 0.0f), scaled(vector2f(pos.x,        pos.y+size.y)));
	va.Add(vector3f(1.0f, 0.0f, 0.0f), scaled(vector2f(pos.x+size.x, pos.y)));
	va.Add(vector3f(1.0f, 1.0f, 0.0f), scaled(vector2f(pos.x+size.x, pos.y+size.y)));

	vbuffer.Reset( CreatePosUVVB(va.GetNumVerts(), material, renderer) );
	SkinVert* vtxPtr = vbuffer->Map<SkinVert>(Graphics::BUFFER_MAP_WRITE);
	assert(vbuffer->GetDesc().stride == sizeof(SkinVert));
	for(Uint32 i=0 ; i<va.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= va.position[i];
		vtxPtr[i].uv	= va.uv0[i];
	}
	vbuffer->Unmap();
}

void Skin::BorderedRectElement::GenerateVertexBuffer(Graphics::Renderer* renderer, Graphics::Material* material)
{
	// for uv coordinates
	const float width = borderWidth;
	const float height = borderHeight;
	// for position coordinates (which are subject to positioning and scaling)
	const float invw = 1.0f / float(borderWidth);
	const float invh = 1.0f / float(borderHeight);

	Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_UV0);
	
	va.Add(vector3f(0.0f,           0.0f,        0.0f), scaled(vector2f(pos.x,              pos.y)));
	va.Add(vector3f(0.0f,           invh,        0.0f), scaled(vector2f(pos.x,              pos.y+height)));
	va.Add(vector3f(invw,           0.0f,        0.0f), scaled(vector2f(pos.x+width,        pos.y)));
	va.Add(vector3f(invw,           invh,        0.0f), scaled(vector2f(pos.x+width,        pos.y+height)));
	va.Add(vector3f(1.0f-invw,      0.0f,        0.0f), scaled(vector2f(pos.x+size.x-width, pos.y)));
	va.Add(vector3f(1.0f-invw,      invh,        0.0f), scaled(vector2f(pos.x+size.x-width, pos.y+height)));
	va.Add(vector3f(1.0f,           0.0f,        0.0f), scaled(vector2f(pos.x+size.x,       pos.y)));
	va.Add(vector3f(1.0f,           invh,        0.0f), scaled(vector2f(pos.x+size.x,       pos.y+height)));

	// degenerate triangles to join rows
	va.Add(vector3f(1.0f,           invh,        0.0f), scaled(vector2f(pos.x+size.x,       pos.y+height)));
	va.Add(vector3f(0.0f,           invh,        0.0f), scaled(vector2f(pos.x,              pos.y+height)));

	va.Add(vector3f(0.0f,           invh,        0.0f), scaled(vector2f(pos.x,              pos.y+height)));
	va.Add(vector3f(0.0f,           1.0f-invh,   0.0f), scaled(vector2f(pos.x,              pos.y+size.y-height)));
	va.Add(vector3f(invw,           invh,        0.0f), scaled(vector2f(pos.x+width,        pos.y+height)));
	va.Add(vector3f(invw,           1.0f-invh,   0.0f), scaled(vector2f(pos.x+width,        pos.y+size.y-height)));
	va.Add(vector3f(1.0f-invw,      invh,        0.0f), scaled(vector2f(pos.x+size.x-width, pos.y+height)));
	va.Add(vector3f(1.0f-invw,      1.0f-invh,   0.0f), scaled(vector2f(pos.x+size.x-width, pos.y+size.y-height)));
	va.Add(vector3f(1.0f,           invh,        0.0f), scaled(vector2f(pos.x+size.x,       pos.y+height)));
	va.Add(vector3f(1.0f,           1.0f-invh,   0.0f), scaled(vector2f(pos.x+size.x,       pos.y+size.y-height)));

	// degenerate triangles to join rows
	va.Add(vector3f(1.0f,           1.0f-invh,   0.0f), scaled(vector2f(pos.x+size.x,       pos.y+size.y-height)));
	va.Add(vector3f(0.0f,           1.0f-invh,   0.0f), scaled(vector2f(pos.x,              pos.y+size.y-height)));

	va.Add(vector3f(0.0f,           1.0f-invh,   0.0f), scaled(vector2f(pos.x,              pos.y+size.y-height)));
	va.Add(vector3f(0.0f,           1.0f,        0.0f), scaled(vector2f(pos.x,              pos.y+size.y)));
	va.Add(vector3f(invw,           1.0f-invh,   0.0f), scaled(vector2f(pos.x+width,        pos.y+size.y-height)));
	va.Add(vector3f(invw,           1.0f,        0.0f), scaled(vector2f(pos.x+width,        pos.y+size.y)));
	va.Add(vector3f(1.0f-invw,      1.0f-invh,   0.0f), scaled(vector2f(pos.x+size.x-width, pos.y+size.y-height)));
	va.Add(vector3f(1.0f-invw,      1.0f,        0.0f), scaled(vector2f(pos.x+size.x-width, pos.y+size.y)));
	va.Add(vector3f(1.0f,           1.0f-invh,   0.0f), scaled(vector2f(pos.x+size.x,       pos.y+size.y-height)));
	va.Add(vector3f(1.0f,           1.0f,        0.0f), scaled(vector2f(pos.x+size.x,       pos.y+size.y)));
	
	vbuffer.Reset( CreatePosUVVB(va.GetNumVerts(), material, renderer) );
	SkinVert* vtxPtr = vbuffer->Map<SkinVert>(Graphics::BUFFER_MAP_WRITE);
	assert(vbuffer->GetDesc().stride == sizeof(SkinVert));
	for(Uint32 i=0 ; i<va.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= va.position[i];
		vtxPtr[i].uv	= va.uv0[i];
	}
	vbuffer->Unmap();
}

void Skin::EdgedRectElement::GenerateVertexBuffer(Graphics::Renderer* renderer, Graphics::Material* material, const EdgedOrientation orient)
{
	Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_UV0);

	if( EdgedOrientation::VERTICAL == orient ) {
		// for uv coordinates
		const float height = edgeWidth;
		// for position coordinates (which are subject to positioning and scaling)
		const float invh = 1.0f / float(edgeWidth);

		va.Add(vector3f(1.0f, 0.0f,      0.0f), scaled(vector2f(pos.x+size.x,       pos.y)));
		va.Add(vector3f(0.0f, 0.0f,      0.0f), scaled(vector2f(pos.x,              pos.y)));
		va.Add(vector3f(1.0f, invh,      0.0f), scaled(vector2f(pos.x+size.x,       pos.y+height)));
		va.Add(vector3f(0.0f, invh,      0.0f), scaled(vector2f(pos.x,              pos.y+height)));
		va.Add(vector3f(1.0f, 1.0f-invh, 0.0f), scaled(vector2f(pos.x+size.x,       pos.y+size.y-height)));
		va.Add(vector3f(0.0f, 1.0f-invh, 0.0f), scaled(vector2f(pos.x,              pos.y+size.y-height)));
		va.Add(vector3f(1.0f, 1.0f,      0.0f), scaled(vector2f(pos.x+size.x,       pos.y+size.y)));
		va.Add(vector3f(0.0f, 1.0f,      0.0f), scaled(vector2f(pos.x,              pos.y+size.y)));
	} else {
		// for uv coordinates
		const float width = edgeWidth;
		// for position coordinates (which are subject to positioning and scaling)
		const float invw = 1.0f / float(edgeWidth);

		va.Add(vector3f(0.0f,      0.0f, 0.0f), scaled(vector2f(pos.x,              pos.y)));
		va.Add(vector3f(0.0f,      1.0f, 0.0f), scaled(vector2f(pos.x,              pos.y+size.y)));
		va.Add(vector3f(invw,      0.0f, 0.0f), scaled(vector2f(pos.x+width,        pos.y)));
		va.Add(vector3f(invw,      1.0f, 0.0f), scaled(vector2f(pos.x+width,        pos.y+size.y)));
		va.Add(vector3f(1.0f-invw, 0.0f, 0.0f), scaled(vector2f(pos.x+size.x-width, pos.y)));
		va.Add(vector3f(1.0f-invw, 1.0f, 0.0f), scaled(vector2f(pos.x+size.x-width, pos.y+size.y)));
		va.Add(vector3f(1.0f,      0.0f, 0.0f), scaled(vector2f(pos.x+size.x,       pos.y)));
		va.Add(vector3f(1.0f,      1.0f, 0.0f), scaled(vector2f(pos.x+size.x,       pos.y+size.y)));
	}

	vbuffer.Reset( CreatePosUVVB(va.GetNumVerts(), material, renderer) );
	SkinVert* vtxPtr = vbuffer->Map<SkinVert>(Graphics::BUFFER_MAP_WRITE);
	assert(vbuffer->GetDesc().stride == sizeof(SkinVert));
	for(Uint32 i=0 ; i<va.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= va.position[i];
		vtxPtr[i].uv	= va.uv0[i];
	}
	vbuffer->Unmap();
}

void Skin::DrawRectElement(const RectElement &element, const Point &pos, const Point &size, Graphics::BlendMode blendMode) const
{
	Graphics::Renderer::MatrixTicket mt(m_renderer, Graphics::MatrixMode::MODELVIEW);

	matrix4x4f local(m_renderer->GetCurrentModelView());
	local.Translate(pos.x, pos.y, 0.0f);
	local.Scale(size.x, size.y, 0.0f);
	m_renderer->SetTransform(local);

	m_textureMaterial->diffuse = Color(Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, m_opacity*Color::WHITE.a);
	m_renderer->DrawBuffer(element.GetVertexBuffer(), GetRenderState(blendMode), m_textureMaterial.Get(), Graphics::TRIANGLE_STRIP);
}

void Skin::DrawBorderedRectElement(const BorderedRectElement &element, const Point &pos, const Point &size, Graphics::BlendMode blendMode) const
{
	Graphics::Renderer::MatrixTicket mt(m_renderer, Graphics::MatrixMode::MODELVIEW);

	matrix4x4f local(m_renderer->GetCurrentModelView());
	local.Translate(pos.x, pos.y, 0.0f);
	local.Scale(size.x, size.y, 0.0f);
	m_renderer->SetTransform(local);

	m_textureMaterial->diffuse = Color(Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, m_opacity*Color::WHITE.a);
	m_renderer->DrawBuffer(element.GetVertexBuffer(), GetRenderState(blendMode), m_textureMaterial.Get(), Graphics::TRIANGLE_STRIP);
}

void Skin::DrawVerticalEdgedRectElement(const EdgedRectElement &element, const Point &pos, const Point &size, Graphics::BlendMode blendMode) const
{
	Graphics::Renderer::MatrixTicket mt(m_renderer, Graphics::MatrixMode::MODELVIEW);

	matrix4x4f local(m_renderer->GetCurrentModelView());
	local.Translate(pos.x, pos.y, 0.0f);
	local.Scale(size.x, size.y, 0.0f);
	m_renderer->SetTransform(local);

	m_textureMaterial->diffuse = Color(Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, m_opacity*Color::WHITE.a);
	m_renderer->DrawBuffer(element.GetVertexBuffer(), GetRenderState(blendMode), m_textureMaterial.Get(), Graphics::TRIANGLE_STRIP);
}

void Skin::DrawHorizontalEdgedRectElement(const EdgedRectElement &element, const Point &pos, const Point &size, Graphics::BlendMode blendMode) const
{
	Graphics::Renderer::MatrixTicket mt(m_renderer, Graphics::MatrixMode::MODELVIEW);

	matrix4x4f local(m_renderer->GetCurrentModelView());
	local.Translate(pos.x, pos.y, 0.0f);
	local.Scale(size.x, size.y, 0.0f);
	m_renderer->SetTransform(local);

	m_textureMaterial->diffuse = Color(Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, m_opacity*Color::WHITE.a);
	m_renderer->DrawBuffer(element.GetVertexBuffer(), GetRenderState(blendMode), m_textureMaterial.Get(), Graphics::TRIANGLE_STRIP);
}

void Skin::DrawRectColor(const Color &col, const Point &pos, const Point &size) const
{
	Graphics::VertexArray va(Graphics::ATTRIB_POSITION);

	va.Add(vector3f(pos.x,        pos.y,        0.0f));
	va.Add(vector3f(pos.x,        pos.y+size.y, 0.0f));
	va.Add(vector3f(pos.x+size.x, pos.y,        0.0f));
	va.Add(vector3f(pos.x+size.x, pos.y+size.y, 0.0f));
	
	Graphics::VertexBuffer* vertexBuffer( CreatePosVB(va.GetNumVerts(), m_colorMaterial.Get(), m_renderer) );
	SkinPosOnlyVert* vtxPtr = vertexBuffer->Map<SkinPosOnlyVert>(Graphics::BUFFER_MAP_WRITE);
	assert(vertexBuffer->GetDesc().stride == sizeof(SkinPosOnlyVert));
	for(Uint32 i=0 ; i<va.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= va.position[i];
	}
	vertexBuffer->Unmap();

	m_colorMaterial->diffuse = Color(col.r, col.g, col.b, m_opacity*col.a);
	m_renderer->DrawBuffer(vertexBuffer, GetAlphaBlendState(), m_colorMaterial.Get(), Graphics::TRIANGLE_STRIP);
}

static size_t SplitSpec(const std::string &spec, std::vector<int> &output)
{
	static const std::string delim(",");

	size_t i = 0, start = 0, end = 0;
	while (end != std::string::npos) {
		// get to the first non-delim char
		start = spec.find_first_not_of(delim, end);

		// read the end, no more to do
		if (start == std::string::npos)
			break;

		// find the end - next delim or end of string
		end = spec.find_first_of(delim, start);

		// extract the fragment and remember it
		output[i++] = atoi(spec.substr(start, (end == std::string::npos) ? std::string::npos : end - start).c_str());
	}

	return i;
}

Skin::RectElement Skin::LoadRectElement(const std::string &spec)
{
	std::vector<int> v(4);
	SplitSpec(spec, v);
	RectElement res(v[0], v[1], v[2], v[3]);
	res.GenerateVertexBuffer(m_renderer, m_textureMaterial.Get());
	return res;
}

Skin::BorderedRectElement Skin::LoadBorderedRectElement(const std::string &spec)
{
	std::vector<int> v(8);
	SplitSpec(spec, v);
	BorderedRectElement res(v[0], v[1], v[2], v[3], v[4]*m_scale, v[5]*m_scale, v[6]*m_scale, v[7]*m_scale);
	res.GenerateVertexBuffer(m_renderer, m_textureMaterial.Get());
	return res;
}

Skin::EdgedRectElement Skin::LoadEdgedRectElement(const std::string &spec, const EdgedOrientation orient)
{
	std::vector<int> v(5);
	SplitSpec(spec, v);
	EdgedRectElement res(v[0], v[1], v[2], v[3], v[4]);
	res.GenerateVertexBuffer(m_renderer, m_textureMaterial.Get(), orient);
	return res;
}

}
