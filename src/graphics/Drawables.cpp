// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Drawables.h"
#include "Texture.h"

namespace Graphics {

namespace Drawables {

Circle::Circle(Renderer *renderer, float radius, const vector3f &center, const Color &c, RenderState *state) : m_color(c) {
	m_renderState = state;
	VertexArray vertices (ATTRIB_POSITION);
	for (float theta=0; theta < 2*float(M_PI); theta += 0.05f*float(M_PI)) {
		vertices.Add(vector3f(radius*sin(theta) + center.x, radius*cos(theta) + center.y, center.z));
	}
	SetupVertexBuffer(vertices, renderer);
}

void Circle::Draw(Renderer *renderer) {
	m_material->diffuse = m_color;
	renderer->DrawBuffer(m_vertexBuffer.Get(), m_renderState, m_material.Get(), PrimitiveType::LINE_LOOP);
}

void Circle::SetupVertexBuffer(const Graphics::VertexArray& vertices, Graphics::Renderer *r)
{
	struct CircleVertex {
		vector3f pos;
	};

	MaterialDescriptor desc;
	m_material.Reset(r->CreateMaterial(desc));

	//Create vtx & index buffers and copy data
	VertexBufferDesc vbd;
	vbd.attrib[0].semantic = ATTRIB_POSITION;
	vbd.attrib[0].format   = ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[0].offset   = offsetof(CircleVertex, pos);
	vbd.stride = sizeof(CircleVertex);
	vbd.numVertices = vertices.GetNumVerts();
	vbd.usage = BUFFER_USAGE_STATIC;
	m_material->SetupVertexBufferDesc(vbd);
	m_vertexBuffer.Reset(r->CreateVertexBuffer(vbd));
	CircleVertex* vtxPtr = m_vertexBuffer->Map<CircleVertex>(Graphics::BUFFER_MAP_WRITE);
	assert(m_vertexBuffer->GetDesc().stride == sizeof(CircleVertex));
	for(Uint32 i=0 ; i<vertices.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= vertices.position[i];
	}
	m_vertexBuffer->Unmap();
}

Disk::Disk(Graphics::Renderer *r, Graphics::RenderState *state, const Color &c, float rad)
{
	m_renderState = state;

	VertexArray vertices (ATTRIB_POSITION);
	m_material.Reset(r->CreateMaterial(MaterialDescriptor()));
	m_material->diffuse = c;

	vertices.Add(vector3f(0.f, 0.f, 0.f));
	for (int i = 72; i >= 0; i--) {
		vertices.Add(vector3f(
			0.f+sinf(DEG2RAD(i*5.f))*rad,
			0.f+cosf(DEG2RAD(i*5.f))*rad,
			0.f));
	}

	SetupVertexBuffer(vertices, r);
}

void Disk::Draw(Renderer *r)
{
	r->DrawBuffer(m_vertexBuffer.get(), m_renderState, m_material.Get(), TRIANGLE_FAN);
}

void Disk::SetColor(const Color &c)
{
	m_material->diffuse = c;
}

void Disk::SetupVertexBuffer(const Graphics::VertexArray& vertices, Graphics::Renderer *r)
{
	struct DiskVertex {
		vector3f pos;
	};

	//Create vtx & index buffers and copy data
	VertexBufferDesc vbd;
	vbd.attrib[0].semantic = ATTRIB_POSITION;
	vbd.attrib[0].format   = ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[0].offset   = offsetof(DiskVertex, pos);
	vbd.stride = sizeof(DiskVertex);
	vbd.numVertices = vertices.GetNumVerts();
	vbd.usage = BUFFER_USAGE_STATIC;
	m_material->SetupVertexBufferDesc(vbd);
	m_vertexBuffer.reset(r->CreateVertexBuffer(vbd));
	DiskVertex* vtxPtr = m_vertexBuffer->Map<DiskVertex>(Graphics::BUFFER_MAP_WRITE);
	assert(m_vertexBuffer->GetDesc().stride == sizeof(DiskVertex));
	for(Uint32 i=0 ; i<vertices.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= vertices.position[i];
	}
	m_vertexBuffer->Unmap();
}

Line3D::Line3D() : m_refreshVertexBuffer(true), m_width(2.0f), m_va(new VertexArray(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE, 2))
{
	// XXX bug in Radeon drivers will cause crash in glLineWidth if width >= 3
	m_va->Add(vector3f(0.f), Color(0));
	m_va->Add(vector3f(0.f), Color(255));
}

Line3D::~Line3D()
{
	if( m_va ) {
		delete m_va;
		m_va = nullptr;
	}
}

void Line3D::SetStart(const vector3f &s)
{
	m_va->Set(0, s);
}

void Line3D::SetEnd(const vector3f &e)
{
	m_va->Set(1, e);
}

void Line3D::SetColor(const Color &c)
{
	m_va->Set(0, m_va->position[0], c);
	m_va->Set(1, m_va->position[1], c * 0.5);
}

void Line3D::Draw(Renderer *r, RenderState *rs)
{
	if( !m_vertexBuffer.Valid() ) {
		CreateVertexBuffer(r, 2);
	}
	if( m_refreshVertexBuffer ) {
		m_refreshVertexBuffer = false;
		m_vertexBuffer->Populate( *m_va );
	}
	// XXX would be nicer to draw this as a textured triangle strip
	// can't guarantee linewidth support
	glLineWidth(m_width);
	r->DrawBuffer(m_vertexBuffer.Get(), rs, m_material.Get(), Graphics::LINE_SINGLE);
	glLineWidth(1.f);
}

void Line3D::CreateVertexBuffer(Graphics::Renderer *r, const Uint32 size)
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
	m_vertexBuffer.Reset(r->CreateVertexBuffer(vbd));
}

Lines::Lines() : m_refreshVertexBuffer(true), m_width(2.0f), m_va(new VertexArray(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE))
{
	// XXX bug in Radeon drivers will cause crash in glLineWidth if width >= 3
}

void Lines::SetData(const int vertCount, const vector3f *vertices, const Color &color)
{
	assert( (vertCount & 1) == 0 ); // ensure vertCount is a multiple of 2
	assert( vertices );

	// somethings changed so even if the number of verts is constant the data must be uploaded
	m_refreshVertexBuffer = true;

	// if the number of vert mismatches then clear the current vertex buffer
	if( m_vertexBuffer.Valid() && m_vertexBuffer->GetVertexCount() != vertCount ) {
		// a new one will be created when it is drawn
		m_vertexBuffer.Reset();
	}

	// populate the VertexArray
	m_va->Clear();
	for( int i=0; i<vertCount; i++ ) {
		m_va->Add(vertices[i], color);
	}
}

void Lines::Draw(Renderer *r, RenderState *rs, const PrimitiveType pt)
{
	if( !m_vertexBuffer.Valid() ) {
		CreateVertexBuffer(r, m_va->GetNumVerts());
	}
	if( m_refreshVertexBuffer ) {
		m_refreshVertexBuffer = false;
		m_vertexBuffer->Populate( *m_va );
	}
	// XXX would be nicer to draw this as a textured triangle strip
	// can't guarantee linewidth support
	glLineWidth(m_width);
	r->DrawBuffer(m_vertexBuffer.Get(), rs, m_material.Get(), pt);
	glLineWidth(1.f);
}

void Lines::CreateVertexBuffer(Graphics::Renderer *r, const Uint32 size)
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
	m_vertexBuffer.Reset(r->CreateVertexBuffer(vbd));
}

static const float ICOSX = 0.525731112119133f;
static const float ICOSZ = 0.850650808352039f;

static const vector3f icosahedron_vertices[12] = {
	vector3f(-ICOSX, 0.0, ICOSZ), vector3f(ICOSX, 0.0, ICOSZ), vector3f(-ICOSX, 0.0, -ICOSZ), vector3f(ICOSX, 0.0, -ICOSZ),
	vector3f(0.0, ICOSZ, ICOSX), vector3f(0.0, ICOSZ, -ICOSX), vector3f(0.0, -ICOSZ, ICOSX), vector3f(0.0, -ICOSZ, -ICOSX),
	vector3f(ICOSZ, ICOSX, 0.0), vector3f(-ICOSZ, ICOSX, 0.0), vector3f(ICOSZ, -ICOSX, 0.0), vector3f(-ICOSZ, -ICOSX, 0.0)
};

static const int icosahedron_faces[20][3] = {
	{0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
	{8,10,1}, {8,3,10},{5,3,8}, {5,2,3}, {2,7,3},
	{7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
	{6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11}
};

#pragma pack(push, 4)
struct Sphere3DVertex {
	vector3f pos;
	vector3f nrm;
	vector2f uv;
};
#pragma pack(pop)

Sphere3D::Sphere3D(Renderer *renderer, RefCountedPtr<Material> mat, Graphics::RenderState *state, int subdivs, float scale)
{
	m_material = mat;
	m_renderState = state;

	subdivs = Clamp(subdivs, 0, 4);
	scale = fabs(scale);
	matrix4x4f trans = matrix4x4f::Identity();
	trans.Scale(scale, scale, scale);

	//m_surface.reset(new Surface(TRIANGLES, new VertexArray(ATTRIB_POSITION | ATTRIB_NORMAL | ATTRIB_UV0), mat));
	//reserve some data
	VertexArray vts(ATTRIB_POSITION | ATTRIB_NORMAL | ATTRIB_UV0, 256);
	std::vector<Uint16> indices;

	//initial vertices
	int vi[12];
	for (int i=0; i<12; i++) {
		const vector3f &v = icosahedron_vertices[i];
		vi[i] = AddVertex(vts, trans * v, v);
	}

	//subdivide
	for (int i=0; i<20; i++) {
		Subdivide(vts, indices, trans, icosahedron_vertices[icosahedron_faces[i][0]],
				icosahedron_vertices[icosahedron_faces[i][1]],
				icosahedron_vertices[icosahedron_faces[i][2]],
				vi[icosahedron_faces[i][0]],
				vi[icosahedron_faces[i][1]],
				vi[icosahedron_faces[i][2]],
				subdivs);
	}

	//Create vtx & index buffers and copy data
	VertexBufferDesc vbd;
	vbd.attrib[0].semantic = ATTRIB_POSITION;
	vbd.attrib[0].format   = ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[0].offset   = offsetof(Sphere3DVertex, pos);
	vbd.attrib[1].semantic = ATTRIB_NORMAL;
	vbd.attrib[1].format   = ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[1].offset   = offsetof(Sphere3DVertex, nrm);
	vbd.attrib[2].semantic = ATTRIB_UV0;
	vbd.attrib[2].format   = ATTRIB_FORMAT_FLOAT2;
	vbd.attrib[2].offset   = offsetof(Sphere3DVertex, uv);
	vbd.stride = sizeof(Sphere3DVertex);
	vbd.numVertices = vts.GetNumVerts();
	vbd.usage = BUFFER_USAGE_STATIC;
	m_material->SetupVertexBufferDesc(vbd);
	m_vertexBuffer.reset(renderer->CreateVertexBuffer(vbd));

	auto vtxPtr = m_vertexBuffer->Map<Sphere3DVertex>(Graphics::BUFFER_MAP_WRITE);
	for (Uint32 i = 0; i < vts.GetNumVerts(); i++) {
		vtxPtr->pos = vts.position[i];
		vtxPtr->nrm = vts.normal[i];
		vtxPtr->uv = vts.uv0[i];
		vtxPtr++;
	}
	m_vertexBuffer->Unmap();

	m_indexBuffer.reset(renderer->CreateIndexBuffer(indices.size(), BUFFER_USAGE_STATIC));
	Uint16 *idxPtr = m_indexBuffer->Map(Graphics::BUFFER_MAP_WRITE);
	for (auto it : indices) {
		*idxPtr = it;
		idxPtr++;
	}
	m_indexBuffer->Unmap();
}

void Sphere3D::Draw(Renderer *r)
{
	r->DrawBufferIndexed(m_vertexBuffer.get(), m_indexBuffer.get(), m_renderState, m_material.Get());
}

int Sphere3D::AddVertex(VertexArray &vts, const vector3f &v, const vector3f &n)
{
	vts.position.push_back(v);
	vts.normal.push_back(n);
	//http://www.mvps.org/directx/articles/spheremap.htm
	vts.uv0.push_back(vector2f(asinf(n.x)/M_PI+0.5f, asinf(n.y)/M_PI+0.5f));
	return vts.GetNumVerts() - 1;
}

void Sphere3D::AddTriangle(std::vector<Uint16> &indices, int i1, int i2, int i3)
{
	indices.push_back(i1);
	indices.push_back(i2);
	indices.push_back(i3);
}

void Sphere3D::Subdivide(VertexArray &vts, std::vector<Uint16> &indices,
		const matrix4x4f &trans, const vector3f &v1, const vector3f &v2, const vector3f &v3,
		const int i1, const int i2, const int i3, int depth)
{
	if (depth == 0) {
		AddTriangle(indices, i1, i3, i2);
		return;
	}

	const vector3f v12 = (v1+v2).Normalized();
	const vector3f v23 = (v2+v3).Normalized();
	const vector3f v31 = (v3+v1).Normalized();
	const int i12 = AddVertex(vts, trans * v12, v12);
	const int i23 = AddVertex(vts, trans * v23, v23);
	const int i31 = AddVertex(vts, trans * v31, v31);
	Subdivide(vts, indices, trans, v1, v12, v31, i1, i12, i31, depth-1);
	Subdivide(vts, indices, trans, v2, v23, v12, i2, i23, i12, depth-1);
	Subdivide(vts, indices, trans, v3, v31, v23, i3, i31, i23, depth-1);
	Subdivide(vts, indices, trans, v12, v23, v31, i12, i23, i31, depth-1);
}

// a textured quad with reversed winding
TexturedQuad::TexturedQuad(Graphics::Renderer *r, Graphics::Texture *texture, const vector2f &pos, const vector2f &size, RenderState *state)
	: m_texture(RefCountedPtr<Graphics::Texture>(texture))
{
	assert(state);
	m_renderState = state;

	VertexArray vertices(ATTRIB_POSITION | ATTRIB_UV0);
	Graphics::MaterialDescriptor desc;
	desc.textures = 1;
	m_material.reset(r->CreateMaterial(desc));
	m_material->texture0 = m_texture.Get();

	// these might need to be reversed
	const vector2f texPos = vector2f(0.0f);
	const vector2f texSize = m_texture->GetDescriptor().texSize;

	vertices.Add(vector3f(pos.x,        pos.y,        0.0f), vector2f(texPos.x,           texPos.y+texSize.y));
	vertices.Add(vector3f(pos.x,        pos.y+size.y, 0.0f), vector2f(texPos.x,           texPos.y));
	vertices.Add(vector3f(pos.x+size.x, pos.y,        0.0f), vector2f(texPos.x+texSize.x, texPos.y+texSize.y));
	vertices.Add(vector3f(pos.x+size.x, pos.y+size.y, 0.0f), vector2f(texPos.x+texSize.x, texPos.y));
	
	#pragma pack(push, 4)
	struct QuadVertex {
		vector3f pos;
		vector2f uv;
	};
	#pragma pack(pop)

	//Create vtx & index buffers and copy data
	VertexBufferDesc vbd;
	vbd.attrib[0].semantic = ATTRIB_POSITION;
	vbd.attrib[0].format   = ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[0].offset   = offsetof(QuadVertex, pos);
	vbd.attrib[1].semantic = ATTRIB_UV0;
	vbd.attrib[1].format   = ATTRIB_FORMAT_FLOAT2;
	vbd.attrib[1].offset   = offsetof(QuadVertex, uv);
	vbd.stride = sizeof(QuadVertex);
	vbd.numVertices = vertices.GetNumVerts();
	vbd.usage = BUFFER_USAGE_STATIC;
	m_material->SetupVertexBufferDesc( vbd );
	m_vertexBuffer.reset(r->CreateVertexBuffer(vbd));

	QuadVertex* vtxPtr = m_vertexBuffer->Map<QuadVertex>(Graphics::BUFFER_MAP_WRITE);
	assert(m_vertexBuffer->GetDesc().stride == sizeof(QuadVertex));
	for(Uint32 i=0 ; i<vertices.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= vertices.position[i];
		vtxPtr[i].uv	= vertices.uv0[i];
	}
	m_vertexBuffer->Unmap();
}

void TexturedQuad::Draw(Graphics::Renderer *r)
{
	r->DrawBuffer(m_vertexBuffer.get(), m_renderState, m_material.get(), TRIANGLE_STRIP);
}

Axes3D::Axes3D(Graphics::Renderer *r, Graphics::RenderState *state)
{
	if( state ) {
		m_renderState = state;
	} else {
		Graphics::RenderStateDesc rsd;
		m_renderState = r->CreateRenderState(rsd);
	}

	VertexArray vertices(ATTRIB_POSITION | ATTRIB_DIFFUSE);
	Graphics::MaterialDescriptor desc;
	desc.vertexColors = true;
	m_material.Reset(r->CreateMaterial(desc));
	
	//Draw plain XYZ axes using the current transform
	static const vector3f vtsXYZ[] = {
		vector3f(0.f, 0.f, 0.f),
		vector3f(1.f, 0.f, 0.f),
		vector3f(0.f, 0.f, 0.f),
		vector3f(0.f, 1.f, 0.f),
		vector3f(0.f, 0.f, 0.f),
		vector3f(0.f, 0.f, 1.f),
	};
	static const Color colors[] = {
		Color::RED,
		Color::RED,
		Color::BLUE,
		Color::BLUE,
		Color::GREEN,
		Color::GREEN,
	};

	for( int i=0 ; i<6 ; i++ ) {
		vertices.Add( vtsXYZ[i], colors[i] );
	}

	struct AxesVertex {
		vector3f pos;
		Color col;
	};

	//Create vtx & index buffers and copy data
	VertexBufferDesc vbd;
	vbd.attrib[0].semantic = ATTRIB_POSITION;
	vbd.attrib[0].format   = ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[0].offset   = offsetof(AxesVertex, pos);
	vbd.attrib[1].semantic = ATTRIB_DIFFUSE;
	vbd.attrib[1].format   = ATTRIB_FORMAT_UBYTE4;
	vbd.attrib[1].offset   = offsetof(AxesVertex, col);
	vbd.stride = sizeof(AxesVertex);
	vbd.numVertices = vertices.GetNumVerts();
	vbd.usage = BUFFER_USAGE_STATIC;
	m_material->SetupVertexBufferDesc( vbd );
	m_vertexBuffer.Reset(r->CreateVertexBuffer(vbd));
	AxesVertex* vtxPtr = m_vertexBuffer->Map<AxesVertex>(Graphics::BUFFER_MAP_WRITE);
	assert(m_vertexBuffer->GetDesc().stride == sizeof(AxesVertex));
	for(Uint32 i=0 ; i<vertices.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= vertices.position[i];
		vtxPtr[i].col	= vertices.diffuse[i];
	}
	m_vertexBuffer->Unmap();
}

void Axes3D::Draw(Graphics::Renderer *r)
{
	r->DrawBuffer(m_vertexBuffer.Get(), m_renderState, m_material.Get(), LINE_SINGLE);
}

static Axes3D *s_axes = nullptr; 
Axes3D* GetAxes3DDrawable(Graphics::Renderer *r) {
	if( !s_axes ) {
		s_axes = new Axes3D(r);
	}
	return s_axes;
}

}

}
