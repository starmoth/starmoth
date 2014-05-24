// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _RENDERER_PIGL_H
#define _RENDERER_PIGL_H
/*
 * OpenGL 2.X renderer (2.0, GLSL 1.10 at the moment)
 *  - no fixed function support (shaders for everything)
 *  The plan is: make this more like GL3/ES2
 *  - try to stick to bufferobjects
 *  - use glvertexattribpointer instead of glvertexpointer etc
 *  - get rid of built-in glMaterial, glMatrix use
 */
#include "graphics/Renderer.h"
#include <stack>
#include <unordered_map>

namespace Graphics {

class Texture;
struct Settings;

namespace PiGL {
	class GasGiantSurfaceMaterial;
	class GeoSphereSkyMaterial;
	class GeoSphereSurfaceMaterial;
	class Material;
	class MultiMaterial;
	class LitMultiMaterial;
	class Program;
	class RenderState;
	class RenderTarget;
	class RingMaterial;
	class FresnelColourMaterial;
	class ShieldMaterial;
	class UIMaterial;
}

class RendererGL : public Renderer
{
public:
	RendererGL(WindowSDL *window, const Graphics::Settings &vs);
	virtual ~RendererGL();

	virtual const char* GetName() const { return "PiGL renderer"; }
	virtual bool GetNearFarRange(float &near, float &far) const;

	virtual bool BeginFrame();
	virtual bool EndFrame();
	virtual bool SwapBuffers();

	virtual bool SetRenderState(RenderState*) override;
	virtual bool SetRenderTarget(RenderTarget*) override;

	virtual bool ClearScreen();
	virtual bool ClearDepthBuffer();
	virtual bool SetClearColor(const Color4f &c);

	virtual bool SetViewport(int x, int y, int width, int height);

	virtual bool SetTransform(const matrix4x4d &m);
	virtual bool SetTransform(const matrix4x4f &m);
	virtual bool SetPerspectiveProjection(float fov, float aspect, float near, float far);
	virtual bool SetOrthographicProjection(float xmin, float xmax, float ymin, float ymax, float zmin, float zmax);
	virtual bool SetProjection(const matrix4x4f &m);

	virtual bool SetWireFrameMode(bool enabled);

	virtual bool SetLights(const int numlights, const Light *l);
	virtual int GetNumLights() const { return m_numLights; }
	virtual bool SetAmbientColor(const Color &c);

	virtual bool SetScissor(bool enabled, const vector2f &pos = vector2f(0.0f), const vector2f &size = vector2f(0.0f));

	virtual bool DrawLines(int vertCount, const vector3f *vertices, const Color *colors, RenderState*, LineType type=LINE_SINGLE) override;
	virtual bool DrawLines(int vertCount, const vector3f *vertices, const Color &color, RenderState*, LineType type=LINE_SINGLE) override;
	virtual bool DrawLines2D(int vertCount, const vector2f *vertices, const Color &color, RenderState*, LineType type=LINE_SINGLE) override;
	virtual bool DrawPoints(int count, const vector3f *points, const Color *colors, RenderState*, float pointSize=1.f) override;
	//virtual bool DrawTriangles(const VertexArray *vertices, RenderState *state, Material *material, PrimitiveType type=TRIANGLES) override;
	virtual bool DrawPointSprites(int count, const vector3f *positions, RenderState *rs, Material *material, float size) override;
	virtual bool DrawBuffer(VertexBuffer*, RenderState*, Material*, const PrimitiveType) override;
	virtual bool DrawBufferIndexed(VertexBuffer*, IndexBuffer*, RenderState*, Material*, const PrimitiveType) override;

	virtual Material *CreateMaterial(const MaterialDescriptor &descriptor) override;
	virtual Texture *CreateTexture(const TextureDescriptor &descriptor) override;
	virtual RenderState *CreateRenderState(const RenderStateDesc &) override;
	virtual RenderTarget *CreateRenderTarget(const RenderTargetDesc &) override;
	virtual VertexBuffer *CreateVertexBuffer(const VertexBufferDesc&) override;
	virtual IndexBuffer *CreateIndexBuffer(Uint32 size, BufferUsage) override;

	virtual bool ReloadShaders();

	virtual bool PrintDebugInfo(std::ostream &out);

	virtual const matrix4x4f& GetCurrentModelView() const { return m_modelViewStack.top(); }
	virtual const matrix4x4f& GetCurrentProjection() const { return m_projectionStack.top(); }
	virtual void GetCurrentViewport(Sint32 *vp) const {
		const Viewport &cur = m_viewportStack.top();
		vp[0] = cur.x; vp[1] = cur.y; vp[2] = cur.w; vp[3] = cur.h;
	}

	virtual void SetMatrixMode(MatrixMode mm);
	virtual void PushMatrix();
	virtual void PopMatrix();
	virtual void LoadIdentity();
	virtual void LoadMatrix(const matrix4x4f &m);
	virtual void Translate( const float x, const float y, const float z );
	virtual void Scale( const float x, const float y, const float z );

protected:
	virtual void PushState();
	virtual void PopState();

	//figure out states from a vertex array and enable them
	//also sets vertex pointers
	void EnableClientStates(const VertexArray*, const Material *);
	void EnableVertexAttributes(const VertexBuffer*);
	int m_numLights;
	int m_numDirLights;
	float m_minZNear;
	float m_maxZFar;

	matrix4x4f& GetCurrentTransform() { return m_currentTransform; }
	matrix4x4f m_currentTransform;

	void SetMaterialShaderTransforms(Material *);
	void SetProgramShaderTransforms(PiGL::Program *);

	PiGL::Program* GetOrCreateProgram(PiGL::Material*);
	friend class PiGL::Material;
	friend class PiGL::UIMaterial;
	friend class PiGL::GasGiantSurfaceMaterial;
	friend class PiGL::GeoSphereSurfaceMaterial;
	friend class PiGL::GeoSphereSkyMaterial;
	friend class PiGL::MultiMaterial;
	friend class PiGL::LitMultiMaterial;
	friend class PiGL::RingMaterial;
	friend class PiGL::FresnelColourMaterial;
	friend class PiGL::ShieldMaterial;
	std::vector<std::pair<MaterialDescriptor, PiGL::Program*> > m_programs;
	std::unordered_map<Uint32, PiGL::RenderState*> m_renderStates;
	float m_invLogZfarPlus1;
	PiGL::RenderTarget *m_activeRenderTarget;
	RenderState *m_activeRenderState;

	MatrixMode m_matrixMode;
	std::stack<matrix4x4f> m_modelViewStack;
	std::stack<matrix4x4f> m_projectionStack;

	struct Viewport {
		Viewport() : x(0), y(0), w(0), h(0) {}
		Sint32 x, y, w, h;
	};
	std::stack<Viewport> m_viewportStack;
};

}

#endif
