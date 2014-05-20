// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "glRenderer.h"
#include "graphics/Graphics.h"
#include "graphics/Light.h"
#include "graphics/Material.h"
#include "OS.h"
#include "StringF.h"
#include "graphics/Texture.h"
#include "graphics/gl/glTexture.h"
#include "graphics/gl/glMaterial.h"
#include "graphics/VertexArray.h"
#include "graphics/GLDebug.h"
#include "GasGiantMaterial.h"
#include "GeoSphereMaterial.h"
#include "glMaterial.h"
#include "glRenderState.h"
#include "glRenderTarget.h"
#include "glVertexBuffer.h"
#include "MultiMaterial.h"
#include "Program.h"
#include "RingMaterial.h"
#include "StarfieldMaterial.h"
#include "FresnelColourMaterial.h"
#include "ShieldMaterial.h"
#include "SkyboxMaterial.h"
#include "SphereImpostorMaterial.h"
#include "UIMaterial.h"
#include "VtxColorMaterial.h"

#include <stddef.h> //for offsetof
#include <ostream>
#include <sstream>
#include <iterator>

namespace Graphics {
	
static std::string glerr_to_string(GLenum err)
{
	switch (err)
	{
	case GL_INVALID_ENUM:
		return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE:
		return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION:
		return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY:
		return "GL_OUT_OF_MEMORY";
	case GL_STACK_OVERFLOW: //deprecated in GL3
		return "GL_STACK_OVERFLOW";
	case GL_STACK_UNDERFLOW: //deprecated in GL3
		return "GL_STACK_UNDERFLOW";
	case GL_INVALID_FRAMEBUFFER_OPERATION_EXT:
		return "GL_INVALID_FRAMEBUFFER_OPERATION";
	default:
		return stringf("Unknown error 0x0%0{x}", err);
	}
}

void CheckRenderErrors()
{
	GLenum err = glGetError();
	if( err ) {
		std::stringstream ss;
		ss << "OpenGL error(s) during frame:\n";
		while (err != GL_NO_ERROR) {
			ss << glerr_to_string(err) << '\n';
			err = glGetError();
		}
		Warning("%s", ss.str().c_str());
	}
}

typedef std::vector<std::pair<MaterialDescriptor, PiGL::Program*> >::const_iterator ProgramIterator;

// for material-less line and point drawing
PiGL::MultiProgram *vtxColorProg;
PiGL::MultiProgram *flatColorProg;

RendererGL::RendererGL(WindowSDL *window, const Graphics::Settings &vs)
: Renderer(window, window->GetWidth(), window->GetHeight())
, m_numDirLights(0)
//the range is very large due to a "logarithmic z-buffer" trick used
//http://outerra.blogspot.com/2009/08/logarithmic-z-buffer.html
//http://www.gamedev.net/blog/73/entry-2006307-tip-of-the-day-logarithmic-zbuffer-artifacts-fix/
, m_minZNear(0.0001f)
, m_maxZFar(10000000.0f)
, m_invLogZfarPlus1(0.f)
, m_activeRenderTarget(0)
, m_activeRenderState(nullptr)
, m_matrixMode(MatrixMode::MODELVIEW)
{
	m_viewportStack.push(Viewport());

	//XXX bunch of fixed function states here!
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	SetMatrixMode(MatrixMode::MODELVIEW);

	m_modelViewStack.push(mat4x4::Identityf());
	m_projectionStack.push(mat4x4::Identityf());

	SetClearColor(Color4f(0.f, 0.f, 0.f, 0.f));
	SetViewport(0, 0, m_width, m_height);

	if (vs.enableDebugMessages)
		GLDebug::Enable();

	MaterialDescriptor desc;
	flatColorProg = new PiGL::MultiProgram(desc);
	m_programs.push_back(std::make_pair(desc, flatColorProg));
	desc.vertexColors = true;
	vtxColorProg = new PiGL::MultiProgram(desc);
	m_programs.push_back(std::make_pair(desc, vtxColorProg));
}

RendererGL::~RendererGL()
{
	while (!m_programs.empty()) delete m_programs.back().second, m_programs.pop_back();
	for (auto state : m_renderStates)
		delete state.second;
}

bool RendererGL::GetNearFarRange(float &near, float &far) const
{
	near = m_minZNear;
	far = m_maxZFar;
	return true;
}

bool RendererGL::BeginFrame()
{
	PROFILE_SCOPED()
	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	return true;
}

bool RendererGL::EndFrame()
{
	return true;
}

bool RendererGL::SwapBuffers()
{
	PROFILE_SCOPED()
#ifndef NDEBUG
	// Check if an error occurred during the frame. This is not very useful for
	// determining *where* the error happened. For that purpose, try GDebugger or
	// the GL_KHR_DEBUG extension
	GLenum err;
	err = glGetError();
	if (err != GL_NO_ERROR) {
		std::stringstream ss;
		ss << "OpenGL error(s) during frame:\n";
		while (err != GL_NO_ERROR) {
			ss << glerr_to_string(err) << '\n';
			err = glGetError();
		}
		Error("%s", ss.str().c_str());
	}
#endif

	GetWindow()->SwapBuffers();
	return true;
}

bool RendererGL::SetRenderState(RenderState *rs)
{
	if (m_activeRenderState != rs) {
		static_cast<PiGL::RenderState*>(rs)->Apply();
		CheckRenderErrors();
		m_activeRenderState = rs;
	}
	return true;
}

bool RendererGL::SetRenderTarget(RenderTarget *rt)
{
	PROFILE_SCOPED()
	if (rt)
		static_cast<PiGL::RenderTarget*>(rt)->Bind();
	else if (m_activeRenderTarget)
		m_activeRenderTarget->Unbind();

	CheckRenderErrors();

	m_activeRenderTarget = static_cast<PiGL::RenderTarget*>(rt);

	return true;
}

bool RendererGL::ClearScreen()
{
	m_activeRenderState = nullptr;
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	CheckRenderErrors();

	return true;
}

bool RendererGL::ClearDepthBuffer()
{
	m_activeRenderState = nullptr;
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
	CheckRenderErrors();

	return true;
}

bool RendererGL::SetClearColor(const Color4f &c)
{
	glClearColor(c.r, c.g, c.b, c.a);
	return true;
}

bool RendererGL::SetViewport(int x, int y, int width, int height)
{
	assert(!m_viewportStack.empty());
	Viewport& currentViewport = m_viewportStack.top();
	currentViewport.x = x;
	currentViewport.y = y;
	currentViewport.w = width;
	currentViewport.h = height;
	glViewport(x, y, width, height);
	CheckRenderErrors();
	return true;
}

bool RendererGL::SetTransform(const matrix4x4d &m)
{
	PROFILE_SCOPED()
	matrix4x4f mf;
	matrix4x4dtof(m, mf);
	return SetTransform(mf);
}

bool RendererGL::SetTransform(const matrix4x4f &m)
{
	PROFILE_SCOPED()
	//same as above
	m_modelViewStack.top() = m;
	SetMatrixMode(MatrixMode::MODELVIEW);
	LoadMatrix(&m[0]);
	CheckRenderErrors();
	return true;
}

bool RendererGL::SetPerspectiveProjection(float fov, float aspect, float near, float far)
{
	PROFILE_SCOPED()

	// update values for log-z hack
	m_invLogZfarPlus1 = 1.0f / (log(far+1.0f)/log(2.0f));

	Graphics::SetFov(fov);

	float ymax = near * tan(fov * M_PI / 360.0);
	float ymin = -ymax;
	float xmin = ymin * aspect;
	float xmax = ymax * aspect;

	const matrix4x4f frustrumMat = matrix4x4f::FrustumMatrix(xmin, xmax, ymin, ymax, near, far);
	SetProjection(frustrumMat);
	return true;
}

bool RendererGL::SetOrthographicProjection(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax)
{
	PROFILE_SCOPED()
	const matrix4x4f orthoMat = matrix4x4f::OrthoFrustum(xmin, xmax, ymin, ymax, zmin, zmax);
	SetProjection(orthoMat);
	return true;
}

bool RendererGL::SetProjection(const matrix4x4f &m)
{
	PROFILE_SCOPED()
	//same as above
	m_projectionStack.top() = m;
	SetMatrixMode(MatrixMode::PROJECTION);
	LoadMatrix(&m[0]);
	CheckRenderErrors();
	return true;
}

bool RendererGL::SetWireFrameMode(bool enabled)
{
	glPolygonMode(GL_FRONT_AND_BACK, enabled ? GL_LINE : GL_FILL);
	CheckRenderErrors();
	return true;
}

bool RendererGL::SetLights(const int numlights, const Light *lights)
{
	if (numlights < 1) return false;

	const int NumLights = std::min(numlights, int(TOTAL_NUM_LIGHTS));

	// XXX move lighting out to shaders

	//glLight depends on the current transform, but we have always
	//relied on it being identity when setting lights.
	Graphics::Renderer::MatrixTicket ticket(this, MatrixMode::MODELVIEW);
	SetTransform(mat4x4::Identityf());

	m_numLights = NumLights;
	m_numDirLights = 0;

	for (int i=0; i<NumLights; i++) {
		const Light &l = lights[i];
		m_lights[i].SetPosition( l.GetPosition() );
		m_lights[i].SetDiffuse( l.GetDiffuse() );
		m_lights[i].SetSpecular( l.GetSpecular() );

		if (l.GetType() == Light::LIGHT_DIRECTIONAL)
			m_numDirLights++;

		assert(m_numDirLights <= TOTAL_NUM_LIGHTS);
	}

	return true;
}

bool RendererGL::SetAmbientColor(const Color &c)
{
	m_ambient = c;
	return true;
}

bool RendererGL::SetScissor(bool enabled, const vector2f &pos, const vector2f &size)
{
	if (enabled) {
		glScissor(pos.x,pos.y,size.x,size.y);
		glEnable(GL_SCISSOR_TEST);
	} else {
		glDisable(GL_SCISSOR_TEST);
	}
	CheckRenderErrors();
	return true;
}

void RendererGL::SetMaterialShaderTransforms(Material *m)
{
	PiGL::Program* p = (static_cast<const PiGL::Material*>(m))->GetProgram();
	SetProgramShaderTransforms( p );
}

void RendererGL::SetProgramShaderTransforms(PiGL::Program *p)
{
	const matrix4x4f& mv = m_modelViewStack.top();
	const matrix4x4f& proj = m_projectionStack.top();
	const matrix4x4f ViewProjection = proj * mv;
	const matrix3x3f orient(mv.GetOrient());
	const matrix4x4f NormalMatrix(orient.Inverse().Transpose());
	//const matrix4x4f NormalMatrix(mv.Inverse().Transpose());

	p->uProjectionMatrix.Set( proj );
	p->uViewMatrix.Set( mv );
	p->uViewMatrixInverse.Set( mv.Inverse() );
	p->uViewProjectionMatrix.Set( ViewProjection );
	p->uNormalMatrix.Set( NormalMatrix );
}

bool RendererGL::DrawLines(int count, const vector3f *v, const Color *c, RenderState* state, LineType t)
{
	PROFILE_SCOPED()
	if (count < 2 || !v) return false;

	SetRenderState(state);

	vtxColorProg->Use();
	vtxColorProg->invLogZfarPlus1.Set(m_invLogZfarPlus1);

	SetProgramShaderTransforms( vtxColorProg );

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(vector3f), v);
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Color), c);
	glDrawArrays(t, 0, count);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	CheckRenderErrors();

	return true;
}

bool RendererGL::DrawLines(int count, const vector3f *v, const Color &c, RenderState *state, LineType t)
{
	PROFILE_SCOPED()
	if (count < 2 || !v) return false;

	SetRenderState(state);

	flatColorProg->Use();
	flatColorProg->diffuse.Set(c);
	flatColorProg->invLogZfarPlus1.Set(m_invLogZfarPlus1);

	SetProgramShaderTransforms( flatColorProg );

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(vector3f), v);
	glDrawArrays(t, 0, count);
	glDisableClientState(GL_VERTEX_ARRAY);
	CheckRenderErrors();

	return true;
}

bool RendererGL::DrawLines2D(int count, const vector2f *v, const Color &c, Graphics::RenderState* state, LineType t)
{
	if (count < 2 || !v) return false;

	SetRenderState(state);

	flatColorProg->Use();
	flatColorProg->diffuse.Set(c);
	flatColorProg->invLogZfarPlus1.Set(m_invLogZfarPlus1);

	SetProgramShaderTransforms( flatColorProg );

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, sizeof(vector2f), v);
	glDrawArrays(t, 0, count);
	glDisableClientState(GL_VERTEX_ARRAY);
	CheckRenderErrors();

	return true;
}

bool RendererGL::DrawPoints(int count, const vector3f *points, const Color *colors, Graphics::RenderState *state, float size)
{
	if (count < 1 || !points || !colors) return false;

	vtxColorProg->Use();
	vtxColorProg->invLogZfarPlus1.Set(m_invLogZfarPlus1);

	SetProgramShaderTransforms( vtxColorProg );

	SetRenderState(state);

	glPointSize(size);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, points);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, colors);
	glDrawArrays(GL_POINTS, 0, count);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glPointSize(1.f); // XXX wont't be necessary
	CheckRenderErrors();

	return true;
}

bool RendererGL::DrawTriangles(const VertexArray *v, RenderState *rs, Material *m, PrimitiveType t)
{
	return true;
	/*if (!v || v->position.size() < 3) return false;

	SetRenderState(rs);

	m->Apply();

	SetMaterialShaderTransforms(m);

	EnableClientStates(v, m);

	glDrawArrays(t, 0, v->GetNumVerts());

	m->Unapply();
	CheckRenderErrors();

	return true;*/
}

bool RendererGL::DrawPointSprites(int count, const vector3f *positions, RenderState *rs, Material *material, float size)
{
	if (count < 1 || !material || !material->texture0) return false;

	VertexArray va(ATTRIB_POSITION | ATTRIB_UV0, count * 6);

	matrix4x4f rot(GetCurrentModelView());
	rot.ClearToRotOnly();
	rot = rot.Inverse();

	const float sz = 0.5f*size;
	const vector3f rotv1 = rot * vector3f(sz, sz, 0.0f);
	const vector3f rotv2 = rot * vector3f(sz, -sz, 0.0f);
	const vector3f rotv3 = rot * vector3f(-sz, -sz, 0.0f);
	const vector3f rotv4 = rot * vector3f(-sz, sz, 0.0f);

	//do two-triangle quads. Could also do indexed surfaces.
	//PiGL renderer should use actual point sprites
	//(see history of Render.cpp for point code remnants)
	for (int i=0; i<count; i++) {
		const vector3f &pos = positions[i];

		va.Add(pos+rotv4, vector2f(0.f, 0.f)); //top left
		va.Add(pos+rotv3, vector2f(0.f, 1.f)); //bottom left
		va.Add(pos+rotv1, vector2f(1.f, 0.f)); //top right

		va.Add(pos+rotv1, vector2f(1.f, 0.f)); //top right
		va.Add(pos+rotv3, vector2f(0.f, 1.f)); //bottom left
		va.Add(pos+rotv2, vector2f(1.f, 1.f)); //bottom right
	}

	DrawTriangles(&va, rs, material);
	CheckRenderErrors();

	return true;
}

bool RendererGL::DrawBuffer(VertexBuffer* vb, RenderState* state, Material* mat, const PrimitiveType pt)
{
	SetRenderState(state);
	mat->Apply();

	SetMaterialShaderTransforms(mat);

	auto gvb = static_cast<PiGL::VertexBuffer*>(vb);

	glBindVertexArray(gvb->GetVAO());
	glBindBuffer(GL_ARRAY_BUFFER, gvb->GetBuffer());

	EnableVertexAttributes(gvb);

	glDrawArrays(pt, 0, gvb->GetVertexCount());
	CheckRenderErrors();
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return true;
}

bool RendererGL::DrawBufferIndexed(VertexBuffer *vb, IndexBuffer *ib, RenderState *state, Material *mat, const PrimitiveType pt)
{
	SetRenderState(state);
	mat->Apply();

	SetMaterialShaderTransforms(mat);

	auto gvb = static_cast<PiGL::VertexBuffer*>(vb);
	auto gib = static_cast<PiGL::IndexBuffer*>(ib);

	glBindVertexArray(gvb->GetVAO());
	glBindBuffer(GL_ARRAY_BUFFER, gvb->GetBuffer());
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gib->GetBuffer());

	EnableVertexAttributes(gvb);

	glDrawElements(pt, ib->GetIndexCount(), GL_UNSIGNED_SHORT, 0);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return true;
}


void RendererGL::EnableClientStates(const VertexArray *v, const Material *m)
{
	PROFILE_SCOPED();

	/*if (!v) return;
	assert(v->position.size() > 0); //would be strange

	m->GetDescriptor().

	// XXX could be 3D or 2D
	glEnableVertexAttribArray(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, reinterpret_cast<const GLvoid *>(&v->position[0]));

	if (v->HasAttrib(ATTRIB_DIFFUSE)) {
		assert(! v->diffuse.empty());
		glEnableVertexAttribArray(GL_COLOR_ARRAY);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, reinterpret_cast<const GLvoid *>(&v->diffuse[0]));
	}
	if (v->HasAttrib(ATTRIB_NORMAL)) {
		assert(! v->normal.empty());
		glEnableVertexAttribArray(GL_NORMAL_ARRAY);
		glNormalPointer(GL_FLOAT, 0, reinterpret_cast<const GLvoid *>(&v->normal[0]));
	}
	if (v->HasAttrib(ATTRIB_UV0)) {
		assert(! v->uv0.empty());
		glEnableVertexAttribArray(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, reinterpret_cast<const GLvoid *>(&v->uv0[0]));
	}*/
	CheckRenderErrors();
}

void RendererGL::EnableVertexAttributes(const VertexBuffer* gvb)
{
	// Enable the Vertex attributes
	for (Uint8 i = 0; i < MAX_ATTRIBS; i++) {
		const auto& attr  = gvb->GetDesc().attrib[i];
		switch (attr.semantic) {
		case ATTRIB_POSITION:
		case ATTRIB_NORMAL:
		case ATTRIB_DIFFUSE:
		case ATTRIB_UV0:
			glEnableVertexAttribArray(attr.location);	// Enable the attribute at that location
			break;
		case ATTRIB_NONE:
		default:
			break;
		}
	}
}

Material *RendererGL::CreateMaterial(const MaterialDescriptor &d)
{
	PROFILE_SCOPED()
	MaterialDescriptor desc = d;

	PiGL::Material *mat = 0;
	PiGL::Program *p = 0;

	if (desc.lighting) {
		desc.dirLights = m_numDirLights;
	}

	// Create the material. It will be also used to create the shader,
	// like a tiny factory
	switch (desc.effect) {
	case EFFECT_VTXCOLOR:
		mat = new PiGL::VtxColorMaterial();
		break;
	case EFFECT_UI:
		mat = new PiGL::UIMaterial();
		break;
	case EFFECT_PLANETRING:
		mat = new PiGL::RingMaterial();
		break;
	case EFFECT_STARFIELD:
		mat = new PiGL::StarfieldMaterial();
		break;
	case EFFECT_GEOSPHERE_TERRAIN:
	case EFFECT_GEOSPHERE_TERRAIN_WITH_LAVA:
	case EFFECT_GEOSPHERE_TERRAIN_WITH_WATER:
		mat = new PiGL::GeoSphereSurfaceMaterial();
		break;
	case EFFECT_GEOSPHERE_SKY:
		mat = new PiGL::GeoSphereSkyMaterial();
		break;
	case EFFECT_FRESNEL_SPHERE:
		mat = new PiGL::FresnelColourMaterial();
		break;
	case EFFECT_SHIELD:
		mat = new PiGL::ShieldMaterial();
		break;
	case EFFECT_SKYBOX:
		mat = new PiGL::SkyboxMaterial();
		break;
	case EFFECT_SPHEREIMPOSTOR:
		mat = new PiGL::SphereImpostorMaterial();
		break;
	case EFFECT_GASSPHERE_TERRAIN:
		mat = new PiGL::GasGiantSurfaceMaterial();
		break;
	default:
		if (desc.lighting)
			mat = new PiGL::LitMultiMaterial();
		else
			mat = new PiGL::MultiMaterial();
	}

	mat->m_renderer = this;
	mat->m_descriptor = desc;

	p = GetOrCreateProgram(mat); // XXX throws ShaderException on compile/link failure

	mat->SetProgram(p);
	CheckRenderErrors();
	return mat;
}

bool RendererGL::ReloadShaders()
{
	Output("Reloading " SIZET_FMT " programs...\n", m_programs.size());
	for (ProgramIterator it = m_programs.begin(); it != m_programs.end(); ++it) {
		it->second->Reload();
	}
	Output("Done.\n");

	return true;
}

PiGL::Program* RendererGL::GetOrCreateProgram(PiGL::Material *mat)
{
	const MaterialDescriptor &desc = mat->GetDescriptor();
	PiGL::Program *p = 0;

	// Find an existing program...
	for (ProgramIterator it = m_programs.begin(); it != m_programs.end(); ++it) {
		if ((*it).first == desc) {
			p = (*it).second;
			break;
		}
	}

	// ...or create a new one
	if (!p) {
		p = mat->CreateProgram(desc);
		m_programs.push_back(std::make_pair(desc, p));
	}
	CheckRenderErrors();

	return p;
}

Texture *RendererGL::CreateTexture(const TextureDescriptor &descriptor)
{
	return new TextureGL(descriptor, true);
}

RenderState *RendererGL::CreateRenderState(const RenderStateDesc &desc)
{
	const uint32_t hash = lookup3_hashlittle(&desc, sizeof(RenderStateDesc), 0);
	auto it = m_renderStates.find(hash);
	if (it != m_renderStates.end())
		return it->second;
	else {
		auto *rs = new PiGL::RenderState(desc);
		m_renderStates[hash] = rs;
		return rs;
	}
}

RenderTarget *RendererGL::CreateRenderTarget(const RenderTargetDesc &desc)
{
	PiGL::RenderTarget* rt = new PiGL::RenderTarget(desc);
	rt->Bind();
	if (desc.colorFormat != TEXTURE_NONE) {
		Graphics::TextureDescriptor cdesc(
			desc.colorFormat,
			vector2f(desc.width, desc.height),
			vector2f(desc.width, desc.height),
			LINEAR_CLAMP,
			false,
			false);
		TextureGL *colorTex = new TextureGL(cdesc, false);
		rt->SetColorTexture(colorTex);
	}
	if (desc.depthFormat != TEXTURE_NONE) {
		if (desc.allowDepthTexture) {
			Graphics::TextureDescriptor ddesc(
				TEXTURE_DEPTH,
				vector2f(desc.width, desc.height),
				vector2f(desc.width, desc.height),
				LINEAR_CLAMP,
				false,
				false);
			TextureGL *depthTex = new TextureGL(ddesc, false);
			rt->SetDepthTexture(depthTex);
		} else {
			rt->CreateDepthRenderbuffer();
		}
	}
	rt->CheckStatus();
	rt->Unbind();
	CheckRenderErrors();
	return rt;
}

VertexBuffer *RendererGL::CreateVertexBuffer(const VertexBufferDesc &desc)
{
	return new PiGL::VertexBuffer(desc);
}

IndexBuffer *RendererGL::CreateIndexBuffer(Uint32 size, BufferUsage usage)
{
	return new PiGL::IndexBuffer(size, usage);
}

// XXX very heavy. in the future when all GL calls are made through the
// renderer, we can probably do better by trackingn current state and
// only restoring the things that have changed
void RendererGL::PushState()
{
	SetMatrixMode(MatrixMode::PROJECTION);
	PushMatrix();
	SetMatrixMode(MatrixMode::MODELVIEW);
	PushMatrix();
	m_viewportStack.push( m_viewportStack.top() );
	//glPushAttrib(GL_ALL_ATTRIB_BITS & (~GL_POINT_BIT));
	CheckRenderErrors();
}

void RendererGL::PopState()
{
	//glPopAttrib();
	m_viewportStack.pop();
	assert(!m_viewportStack.empty());
	SetMatrixMode(MatrixMode::PROJECTION);
	PopMatrix();
	SetMatrixMode(MatrixMode::MODELVIEW);
	PopMatrix();
	CheckRenderErrors();
}

static void dump_opengl_value(std::ostream &out, const char *name, GLenum id, int num_elems)
{
	assert(num_elems > 0 && num_elems <= 4);
	assert(name);

	GLdouble e[4];
	glGetDoublev(id, e);

	GLenum err = glGetError();
	if (err == GL_NO_ERROR) {
		out << name << " = " << e[0];
		for (int i = 1; i < num_elems; ++i)
			out << ", " << e[i];
		out << "\n";
	} else {
		while (err != GL_NO_ERROR) {
			if (err == GL_INVALID_ENUM) { out << name << " -- not supported\n"; }
			else { out << name << " -- unexpected error (" << err << ") retrieving value\n"; }
			err = glGetError();
		}
	}
}

bool RendererGL::PrintDebugInfo(std::ostream &out)
{
	out << "OpenGL version " << glGetString(GL_VERSION);
	out << ", running on " << glGetString(GL_VENDOR);
	out << " " << glGetString(GL_RENDERER) << "\n";

	out << "GLEW version " << glewGetString(GLEW_VERSION) << "\n";

	if (glewIsSupported("GL_VERSION_2_0"))
		out << "Shading language version: " <<  glGetString(GL_SHADING_LANGUAGE_VERSION_ARB) << "\n";

	out << "Available extensions:" << "\n";
	GLint numext = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &numext);
	if (glewIsSupported("GL_VERSION_3_0")) {
		for (int i = 0; i < numext; ++i) {
			out << "  " << glGetStringi(GL_EXTENSIONS, i) << "\n";
		}
	}
	else {
		out << "  ";
		std::istringstream ext(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));
		std::copy(
			std::istream_iterator<std::string>(ext),
			std::istream_iterator<std::string>(),
			std::ostream_iterator<std::string>(out, "\n  "));
	}

	out << "\nImplementation Limits:\n";

	// first, clear all OpenGL error flags
	while (glGetError() != GL_NO_ERROR) {}

#define DUMP_GL_VALUE(name) dump_opengl_value(out, #name, name, 1)
#define DUMP_GL_VALUE2(name) dump_opengl_value(out, #name, name, 2)

	DUMP_GL_VALUE(GL_MAX_3D_TEXTURE_SIZE);
	//DUMP_GL_VALUE(GL_MAX_ATTRIB_STACK_DEPTH);
	//DUMP_GL_VALUE(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH);
	DUMP_GL_VALUE(GL_MAX_CLIP_PLANES);
	DUMP_GL_VALUE(GL_MAX_COLOR_ATTACHMENTS_EXT);
	//DUMP_GL_VALUE(GL_MAX_COLOR_MATRIX_STACK_DEPTH);
	DUMP_GL_VALUE(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
	DUMP_GL_VALUE(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
	DUMP_GL_VALUE(GL_MAX_DRAW_BUFFERS);
	DUMP_GL_VALUE(GL_MAX_ELEMENTS_INDICES);
	DUMP_GL_VALUE(GL_MAX_ELEMENTS_VERTICES);
	DUMP_GL_VALUE(GL_MAX_EVAL_ORDER);
	DUMP_GL_VALUE(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS);
	DUMP_GL_VALUE(GL_MAX_LIGHTS);
	DUMP_GL_VALUE(GL_MAX_LIST_NESTING);
	DUMP_GL_VALUE(GL_MAX_MODELVIEW_STACK_DEPTH);
	DUMP_GL_VALUE(GL_MAX_NAME_STACK_DEPTH);
	DUMP_GL_VALUE(GL_MAX_PIXEL_MAP_TABLE);
	DUMP_GL_VALUE(GL_MAX_PROJECTION_STACK_DEPTH);
	DUMP_GL_VALUE(GL_MAX_RENDERBUFFER_SIZE_EXT);
	DUMP_GL_VALUE(GL_MAX_SAMPLES_EXT);
	DUMP_GL_VALUE(GL_MAX_TEXTURE_COORDS);
	DUMP_GL_VALUE(GL_MAX_TEXTURE_IMAGE_UNITS);
	DUMP_GL_VALUE(GL_MAX_TEXTURE_LOD_BIAS);
	DUMP_GL_VALUE(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
	DUMP_GL_VALUE(GL_MAX_TEXTURE_SIZE);
	DUMP_GL_VALUE(GL_MAX_TEXTURE_STACK_DEPTH);
	DUMP_GL_VALUE(GL_MAX_TEXTURE_UNITS);
	DUMP_GL_VALUE(GL_MAX_VARYING_FLOATS);
	DUMP_GL_VALUE(GL_MAX_VERTEX_ATTRIBS);
	DUMP_GL_VALUE(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
	DUMP_GL_VALUE(GL_MAX_VERTEX_UNIFORM_COMPONENTS);
	DUMP_GL_VALUE(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
	DUMP_GL_VALUE(GL_SAMPLE_BUFFERS);
	DUMP_GL_VALUE(GL_SAMPLES);
	DUMP_GL_VALUE2(GL_ALIASED_LINE_WIDTH_RANGE);
	DUMP_GL_VALUE2(GL_ALIASED_POINT_SIZE_RANGE);
	DUMP_GL_VALUE2(GL_MAX_VIEWPORT_DIMS);
	DUMP_GL_VALUE2(GL_SMOOTH_LINE_WIDTH_RANGE);
	DUMP_GL_VALUE2(GL_SMOOTH_POINT_SIZE_RANGE);

#undef DUMP_GL_VALUE
#undef DUMP_GL_VALUE2

	return true;
}

void RendererGL::SetMatrixMode(MatrixMode mm)
{
	PROFILE_SCOPED()
	if( mm != m_matrixMode ) {
		m_matrixMode = mm;
	}
	CheckRenderErrors();
}

void RendererGL::PushMatrix()
{
	PROFILE_SCOPED()
	switch(m_matrixMode) {
		case MatrixMode::MODELVIEW:
			m_modelViewStack.push(m_modelViewStack.top());
			break;
		case MatrixMode::PROJECTION:
			m_projectionStack.push(m_projectionStack.top());
			break;
	}
	CheckRenderErrors();
}

void RendererGL::PopMatrix()
{
	PROFILE_SCOPED()
	switch(m_matrixMode) {
		case MatrixMode::MODELVIEW:
			m_modelViewStack.pop();
			assert(m_modelViewStack.size());
			break;
		case MatrixMode::PROJECTION:
			m_projectionStack.pop();
			assert(m_projectionStack.size());
			break;
	}
	CheckRenderErrors();
}

void RendererGL::LoadIdentity()
{
	PROFILE_SCOPED()
	switch(m_matrixMode) {
		case MatrixMode::MODELVIEW:
			m_modelViewStack.top() = mat4x4::Identityf();
			break;
		case MatrixMode::PROJECTION:
			m_projectionStack.top() = mat4x4::Identityf();
			break;
	}
	CheckRenderErrors();
}

void RendererGL::LoadMatrix(const matrix4x4f &m)
{
	PROFILE_SCOPED()
	switch(m_matrixMode) {
		case MatrixMode::MODELVIEW:
			m_modelViewStack.top() = m;
			break;
		case MatrixMode::PROJECTION:
			m_projectionStack.top() = m;
			break;
	}
	CheckRenderErrors();
}

void RendererGL::Translate( const float x, const float y, const float z )
{
	PROFILE_SCOPED()
	switch(m_matrixMode) {
		case MatrixMode::MODELVIEW:
			m_modelViewStack.top().Translate(x,y,z);
			break;
		case MatrixMode::PROJECTION:
			m_projectionStack.top().Translate(x,y,z);
			break;
	}
	CheckRenderErrors();
}

void RendererGL::Scale( const float x, const float y, const float z )
{
	PROFILE_SCOPED()
	switch(m_matrixMode) {
		case MatrixMode::MODELVIEW:
			m_modelViewStack.top().Scale(x,y,z);
			break;
		case MatrixMode::PROJECTION:
			m_modelViewStack.top().Scale(x,y,z);
			break;
	}
	CheckRenderErrors();
}

}
