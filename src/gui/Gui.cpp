// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "libs.h"
#include "Gui.h"
#include "graphics/Graphics.h"
#include "graphics/RenderState.h"

namespace Gui {

namespace RawEvents {
	sigc::signal<void, MouseMotionEvent *> onMouseMotion;
	sigc::signal<void, MouseButtonEvent *> onMouseDown;
	sigc::signal<void, MouseButtonEvent *> onMouseUp;
	sigc::signal<void, SDL_KeyboardEvent *> onKeyDown;
	sigc::signal<void, SDL_KeyboardEvent *> onKeyUp;
	sigc::signal<void, SDL_JoyAxisEvent *> onJoyAxisMotion;
	sigc::signal<void, SDL_JoyButtonEvent *> onJoyButtonDown;
	sigc::signal<void, SDL_JoyButtonEvent *> onJoyButtonUp;
	sigc::signal<void, SDL_JoyHatEvent *> onJoyHatMotion;
}

static Sint32 lastMouseX, lastMouseY;
void HandleSDLEvent(SDL_Event *event)
{
	PROFILE_SCOPED()
	switch (event->type) {
		case SDL_MOUSEBUTTONDOWN:
			lastMouseX = event->button.x;
			lastMouseY = event->button.y;
			Screen::OnClick(&event->button);
			break;
		case SDL_MOUSEBUTTONUP:
			lastMouseX = event->button.x;
			lastMouseY = event->button.y;
			Screen::OnClick(&event->button);
			break;
		case SDL_MOUSEWHEEL: {
			// synthesizing an SDL1.2-style button event for mouse wheels
			SDL_MouseButtonEvent ev;
            ev.type = SDL_MOUSEBUTTONDOWN;
			ev.button = event->wheel.y > 0 ? MouseButtonEvent::BUTTON_WHEELUP : MouseButtonEvent::BUTTON_WHEELDOWN;
			ev.state = SDL_PRESSED;
			ev.x = lastMouseX;
			ev.y = lastMouseY;
			Screen::OnClick(&ev);
			break;
		 }
		case SDL_KEYDOWN:
			Screen::OnKeyDown(&event->key.keysym);
			RawEvents::onKeyDown.emit(&event->key);
			break;
		case SDL_KEYUP:
			Screen::OnKeyUp(&event->key.keysym);
			RawEvents::onKeyUp.emit(&event->key);
			break;
		case SDL_TEXTINPUT:
			Screen::OnTextInput(&event->text);
			break;
		case SDL_MOUSEMOTION:
			lastMouseX = event->motion.x;
			lastMouseY = event->motion.y;
			Screen::OnMouseMotion(&event->motion);
			break;
		case SDL_JOYAXISMOTION:
			RawEvents::onJoyAxisMotion(&event->jaxis);
			break;
		case SDL_JOYBUTTONUP:
			RawEvents::onJoyButtonUp(&event->jbutton);
			break;
		case SDL_JOYBUTTONDOWN:
			RawEvents::onJoyButtonDown(&event->jbutton);
			break;
		case SDL_JOYHATMOTION:
			RawEvents::onJoyHatMotion(&event->jhat);
			break;
	}
}

struct TimerSignal {
	Uint32 goTime;
	sigc::signal<void> sig;
};

static std::list<TimerSignal*> g_timeSignals;

sigc::connection AddTimer(Uint32 ms, sigc::slot<void> slot)
{
	TimerSignal *_s = new TimerSignal;
	_s->goTime = SDL_GetTicks() + ms;
	sigc::connection con = _s->sig.connect(slot);
	g_timeSignals.push_back(_s);
	return con;
}

void Draw()
{
	PROFILE_SCOPED()
	Uint32 t = SDL_GetTicks();
	// also abused like an update() function...
	for (std::list<TimerSignal*>::iterator i = g_timeSignals.begin(); i != g_timeSignals.end();) {
		if (t >= (*i)->goTime) {
			(*i)->sig.emit();
			delete *i;
			i = g_timeSignals.erase(i);
		} else {
			++i;
		}
	}
//	ExpireTimers(t);

	Screen::Draw();
}

void Init(Graphics::Renderer *renderer, int screen_width, int screen_height, int ui_width, int ui_height)
{
	Screen::Init(renderer, screen_width, screen_height, ui_width, ui_height);
}

void Uninit()
{
	std::list<TimerSignal*>::iterator i;
	for (i=g_timeSignals.begin(); i!=g_timeSignals.end(); ++i) delete *i;

	Screen::Uninit();
}

namespace Theme {
	namespace Colors {
		const Color bg(64, 94, 161);
		const Color bgShadow(20, 31, 54);
		const Color tableHeading(178, 178, 255);
	}
	static const float BORDER_WIDTH = 2.0;

	#pragma pack(push, 4)
	struct PosVert { vector3f pos; };
	#pragma pack(pop)

	Graphics::VertexBuffer* GenerateRectVB()
	{
		assert( Screen::GetRenderer() );

		// Setup the array of data
		Graphics::VertexArray va(Graphics::ATTRIB_POSITION, 4);
		va.Add(vector3f(0.0f,1.0f,0.0f));
		va.Add(vector3f(1.0f,1.0f,0.0f));
		va.Add(vector3f(1.0f,0.0f,0.0f));
		va.Add(vector3f(0.0f,0.0f,0.0f));

		// create buffer
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.numVertices = va.GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;
		Screen::flatColorMaterial->SetupVertexBufferDesc( vbd );

		// Upload data
		Graphics::VertexBuffer *vb = Screen::GetRenderer()->CreateVertexBuffer(vbd);
		PosVert* vtxPtr = vb->Map<PosVert>(Graphics::BUFFER_MAP_WRITE);
		assert(vb->GetDesc().stride == sizeof(PosVert));
		for(Uint32 i=0 ; i<va.GetNumVerts() ; i++)
		{
			vtxPtr[i].pos	= va.position[i];
		}
		vb->Unmap();

		return vb;
	}

	Graphics::VertexBuffer* GenerateRoundEdgedRect(const vector2f& size, float rad)
	{
		assert( Screen::GetRenderer() );

		static Graphics::VertexArray vts(Graphics::ATTRIB_POSITION);
		vts.Clear();

		const int STEPS = 6;
		if (rad > 0.5f*std::min(size.x, size.y)) {
			rad = 0.5f*std::min(size.x, size.y);
		}

		// top left
		// bottom left
		for (int i=0; i<=STEPS; i++) {
			float ang = M_PI*0.5f*i/float(STEPS);
			vts.Add(vector3f(rad - rad*cos(ang), (size.y - rad) + rad*sin(ang), 0.f));
		}
		// bottom right
		for (int i=0; i<=STEPS; i++) {
			float ang = M_PI*0.5 + M_PI*0.5f*i/float(STEPS);
			vts.Add(vector3f(size.x - rad - rad*cos(ang), (size.y - rad) + rad*sin(ang), 0.f));
		}
		// top right
		for (int i=0; i<=STEPS; i++) {
			float ang = M_PI + M_PI*0.5f*i/float(STEPS);
			vts.Add(vector3f((size.x - rad) - rad*cos(ang), rad + rad*sin(ang), 0.f));
		}

		// top right
		for (int i=0; i<=STEPS; i++) {
			float ang = M_PI*1.5 + M_PI*0.5f*i/float(STEPS);
			vts.Add(vector3f(rad - rad*cos(ang), rad + rad*sin(ang), 0.f));
		}

		// create buffer
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.numVertices = vts.GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;
		Screen::flatColorMaterial->SetupVertexBufferDesc( vbd );

		// Upload data
		Graphics::VertexBuffer *vb = Screen::GetRenderer()->CreateVertexBuffer(vbd);
		PosVert* vtxPtr = vb->Map<PosVert>(Graphics::BUFFER_MAP_WRITE);
		assert(vb->GetDesc().stride == sizeof(PosVert));
		for(Uint32 i=0 ; i<vts.GetNumVerts() ; i++)
		{
			vtxPtr[i].pos	= vts.position[i];
		}
		vb->Unmap();

		return vb;
	}

	Graphics::IndexBuffer* CreateIndexBuffer(const GLubyte indices[], const Uint32 IndexStart, const Uint32 IndexEnd, const Uint32 NumIndices)
	{
		Graphics::IndexBuffer *ib = (Screen::GetRenderer()->CreateIndexBuffer(NumIndices, Graphics::BUFFER_USAGE_STATIC));
		Uint16* idxPtr = ib->Map(Graphics::BUFFER_MAP_WRITE);
		for (Uint32 j = IndexStart; j < IndexEnd; j++) {
			idxPtr[j] = indices[j];
		}
		ib->Unmap();

		return ib;
	}

	void GenerateIndent(IndentData &id, const vector2f& size)
	{
		const vector3f vertices[] = { 
	/* 0 */	vector3f(0,0,0),
	/* 1 */	vector3f(0,size.y,0),
	/* 2 */	vector3f(size.x,size.y,0),
	/* 3 */	vector3f(size.x,0,0),
	/* 4 */	vector3f(BORDER_WIDTH,BORDER_WIDTH,0),
	/* 5 */	vector3f(BORDER_WIDTH,size.y-BORDER_WIDTH,0),
	/* 6 */	vector3f(size.x-BORDER_WIDTH,size.y-BORDER_WIDTH,0),
	/* 7 */	vector3f(size.x-BORDER_WIDTH,BORDER_WIDTH, 0)
		};
		const GLubyte indices[] = {
			0,1,5, 0,5,4, 0,4,7, 0,7,3,
			3,7,6, 3,6,2, 1,2,6, 1,6,5,
			4,5,6, 4,6,7 };

		// create buffer
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.numVertices = 8;
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;
		Screen::flatColorMaterial->SetupVertexBufferDesc( vbd );

		// Upload data
		id.vb.Reset( Screen::GetRenderer()->CreateVertexBuffer(vbd) );
		PosVert* vtxPtr = id.vb->Map<PosVert>(Graphics::BUFFER_MAP_WRITE);
		assert(id.vb->GetDesc().stride == sizeof(PosVert));
		for(Uint32 i=0 ; i<8 ; i++)
		{
			vtxPtr[i].pos	= vertices[i];
		}
		id.vb->Unmap();

		Uint32 IndexStart = 0;
		Uint32 IndexEnd = 12;
		Uint32 NumIndices = 12;

		id.ib[0].Reset(CreateIndexBuffer(indices, IndexStart, IndexEnd, NumIndices));

		IndexStart += NumIndices;
		NumIndices = 12;
		IndexEnd += NumIndices;

		id.ib[1].Reset(CreateIndexBuffer(indices, IndexStart, IndexEnd, NumIndices));

		IndexStart += NumIndices;
		NumIndices = 6;
		IndexEnd += NumIndices;

		id.ib[2].Reset(CreateIndexBuffer(indices, IndexStart, IndexEnd, NumIndices));
	}

	void GenerateOutdent(IndentData &id, const vector2f& size)
	{
		const vector3f vertices[] = { 
	/* 0 */	vector3f(0,0,0),
	/* 1 */	vector3f(0,size.y,0),
	/* 2 */	vector3f(size.x,size.y,0),
	/* 3 */	vector3f(size.x,0,0),
	/* 4 */	vector3f(BORDER_WIDTH,BORDER_WIDTH,0),
	/* 5 */	vector3f(BORDER_WIDTH,size.y-BORDER_WIDTH,0),
	/* 6 */	vector3f(size.x-BORDER_WIDTH,size.y-BORDER_WIDTH,0),
	/* 7 */	vector3f(size.x-BORDER_WIDTH,BORDER_WIDTH, 0)
		};
		const GLubyte indices[] = {
			0,1,5, 0,5,4, 0,4,7, 0,7,3,
			3,7,6, 3,6,2, 1,2,6, 1,6,5,
			4,5,6, 4,6,7 };

		// create buffer
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.numVertices = 8;
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;
		Screen::flatColorMaterial->SetupVertexBufferDesc( vbd );

		// Upload data
		id.vb.Reset( Screen::GetRenderer()->CreateVertexBuffer(vbd) );
		PosVert* vtxPtr = id.vb->Map<PosVert>(Graphics::BUFFER_MAP_WRITE);
		assert(id.vb->GetDesc().stride == sizeof(PosVert));
		for(Uint32 i=0 ; i<8 ; i++)
		{
			vtxPtr[i].pos	= vertices[i];
		}
		id.vb->Unmap();

		Uint32 IndexStart = 0;
		Uint32 IndexEnd = 12;
		Uint32 NumIndices = 12;

		id.ib[0].Reset(CreateIndexBuffer(indices, IndexStart, IndexEnd, NumIndices));

		IndexStart += NumIndices;
		NumIndices = 12;
		IndexEnd += NumIndices;

		id.ib[1].Reset(CreateIndexBuffer(indices, IndexStart, IndexEnd, NumIndices));

		IndexStart += NumIndices;
		NumIndices = 6;
		IndexEnd += NumIndices;

		id.ib[2].Reset(CreateIndexBuffer(indices, IndexStart, IndexEnd, NumIndices));
	}

	void GenerateHollowRect(RefCountedPtr<Graphics::VertexBuffer> &vb, RefCountedPtr<Graphics::IndexBuffer> &ib, const vector2f& size)
	{
		const vector3f vertices[] = { 
	/* 0 */	vector3f(0,0,0),
	/* 1 */	vector3f(0,size.y,0),
	/* 2 */	vector3f(size.x,size.y,0),
	/* 3 */	vector3f(size.x,0,0),
	/* 4 */	vector3f(BORDER_WIDTH,BORDER_WIDTH,0),
	/* 5 */	vector3f(BORDER_WIDTH,size.y-BORDER_WIDTH,0),
	/* 6 */	vector3f(size.x-BORDER_WIDTH,size.y-BORDER_WIDTH,0),
	/* 7 */	vector3f(size.x-BORDER_WIDTH,BORDER_WIDTH, 0)
		};
		const GLubyte indicesTri[] = {
			0,1,5, 0,5,4, 0,4,7, 0,7,3,
			3,7,6, 3,6,2, 1,2,6, 1,6,5 };

		// create buffer
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.numVertices = 8;
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;
		Screen::flatColorMaterial->SetupVertexBufferDesc( vbd );

		// Upload data
		vb.Reset( Screen::GetRenderer()->CreateVertexBuffer(vbd) );
		PosVert* vtxPtr = vb->Map<PosVert>(Graphics::BUFFER_MAP_WRITE);
		assert(vb->GetDesc().stride == sizeof(PosVert));
		for(Uint32 i=0 ; i<8 ; i++)
		{
			vtxPtr[i].pos	= vertices[i];
		}
		vb->Unmap();

		//create index buffer & copy
		ib.Reset(Screen::GetRenderer()->CreateIndexBuffer(24, Graphics::BUFFER_USAGE_STATIC));
		Uint16* idxPtr = ib->Map(Graphics::BUFFER_MAP_WRITE);
		for (Uint32 j = 0; j < 24; j++) {
			idxPtr[j] = indicesTri[j];
		}
		ib->Unmap();
	}

	void DrawRect(Graphics::VertexBuffer* vb, const vector2f &pos, const vector2f &size, const Color &c, Graphics::RenderState *state)
	{
		Graphics::Renderer* r = Screen::GetRenderer();
		Graphics::Renderer::MatrixTicket mt(r, Graphics::MatrixMode::MODELVIEW);

		matrix4x4f local(r->GetCurrentModelView());
		local.Translate(pos.x, pos.y, 0.0f);
		local.Scale(size.x, size.y, 0.0f);
		r->SetTransform(local);

		Screen::flatColorMaterial->diffuse = c;
		r->DrawBuffer(vb, state, Screen::flatColorMaterial, Graphics::TRIANGLE_FAN);
	}

	void DrawRoundEdgedRect(Graphics::VertexBuffer* vb, const Color &color, Graphics::RenderState *state)
	{
		Screen::flatColorMaterial->diffuse = color;
		Screen::GetRenderer()->DrawBuffer(vb, state, Screen::flatColorMaterial, Graphics::TRIANGLE_FAN);
	}

	void DrawHollowRect(Graphics::VertexBuffer *vb, Graphics::IndexBuffer *ib, const Color &color, Graphics::RenderState *state)
	{
		Screen::flatColorMaterial->diffuse = color;
		Screen::GetRenderer()->DrawBufferIndexed( vb, ib, state, Screen::flatColorMaterial );
	}

	void DrawIndent(const IndentData &id, Graphics::RenderState *state)
	{
		Screen::flatColorMaterial->diffuse = Colors::bgShadow;
		Screen::GetRenderer()->DrawBufferIndexed( id.vb.Get(), id.ib[0].Get(), state, Screen::flatColorMaterial );

		Screen::flatColorMaterial->diffuse = Color(153,153,153,255);
		Screen::GetRenderer()->DrawBufferIndexed( id.vb.Get(), id.ib[1].Get(), state, Screen::flatColorMaterial );

		Screen::flatColorMaterial->diffuse = Colors::bg;
		Screen::GetRenderer()->DrawBufferIndexed( id.vb.Get(), id.ib[2].Get(), state, Screen::flatColorMaterial );
	}

	void DrawOutdent(const IndentData &id, Graphics::RenderState *state)
	{
		/*Screen::GetRenderer()->SetRenderState(state);

		GLfloat vertices[] = { 0,0,
			0,size[1],
			size[0],size[1],
			size[0],0,
			BORDER_WIDTH,BORDER_WIDTH,
			BORDER_WIDTH,size[1]-BORDER_WIDTH,
			size[0]-BORDER_WIDTH,size[1]-BORDER_WIDTH,
			size[0]-BORDER_WIDTH,BORDER_WIDTH };
		GLubyte indices[] = {
			0,1,5,4, 0,4,7,3,
			3,7,6,2, 1,2,6,5,
			4,5,6,7 };
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, vertices);

		Screen::flatColorMaterial->diffuse = Color(153,153,153,255);
		Screen::flatColorMaterial->Apply();
		glDrawElements(GL_QUADS, 8, GL_UNSIGNED_BYTE, indices);

		Screen::flatColorMaterial->diffuse = Colors::bgShadow;
		Screen::flatColorMaterial->Apply();
		glDrawElements(GL_QUADS, 8, GL_UNSIGNED_BYTE, indices+8);

		Screen::flatColorMaterial->diffuse = Colors::bg;
		Screen::flatColorMaterial->Apply();
		glDrawElements(GL_QUADS, 4, GL_UNSIGNED_BYTE, indices+16);
		glDisableClientState(GL_VERTEX_ARRAY);

		Screen::flatColorMaterial->Unapply();*/

		Screen::flatColorMaterial->diffuse = Color(153,153,153,255);
		Screen::GetRenderer()->DrawBufferIndexed( id.vb.Get(), id.ib[0].Get(), state, Screen::flatColorMaterial );

		Screen::flatColorMaterial->diffuse = Colors::bgShadow;
		Screen::GetRenderer()->DrawBufferIndexed( id.vb.Get(), id.ib[1].Get(), state, Screen::flatColorMaterial );

		Screen::flatColorMaterial->diffuse = Colors::bg;
		Screen::GetRenderer()->DrawBufferIndexed( id.vb.Get(), id.ib[2].Get(), state, Screen::flatColorMaterial );
	}
}

}
