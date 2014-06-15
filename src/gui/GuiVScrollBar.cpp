// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "libs.h"
#include "Gui.h"

static const float SCROLLBAR_SIZE = 12.f;
static const float BORDER = 2.f;

namespace Gui {

ScrollBar::ScrollBar(bool isHoriz) : m_isPressed(false), m_isHoriz(isHoriz), m_prevSize(vector2f(0)), m_prevPos(0.0f)
{
	m_eventMask = EVENT_MOUSEDOWN;
	SetSize(SCROLLBAR_SIZE, SCROLLBAR_SIZE);
}

ScrollBar::~ScrollBar()
{
	if (_m_release) _m_release.disconnect();
	if (_m_motion) _m_motion.disconnect();
}

bool ScrollBar::OnMouseDown(MouseButtonEvent *e)
{
	float size[2];
	GetSize(size);
	if (e->button == SDL_BUTTON_LEFT) {
		m_isPressed = true;
		if (m_isHoriz) {
			m_adjustment->SetValue(e->x / float(size[0]));
		} else {
			m_adjustment->SetValue(e->y / float(size[1]));
		}
		_m_release = RawEvents::onMouseUp.connect(sigc::mem_fun(this, &ScrollBar::OnRawMouseUp));
		_m_motion = RawEvents::onMouseMotion.connect(sigc::mem_fun(this, &ScrollBar::OnRawMouseMotion));
	}
	else if (e->button == MouseButtonEvent::BUTTON_WHEELUP || e->button == MouseButtonEvent::BUTTON_WHEELDOWN) {
		float change = e->button == MouseButtonEvent::BUTTON_WHEELUP ? -0.1 : 0.1;
		float pos = m_adjustment->GetValue();
		m_adjustment->SetValue(Clamp(pos+change, 0.0f, 1.0f));
	}
	return false;
}

void ScrollBar::OnRawMouseUp(MouseButtonEvent *e) {
	if (e->button == SDL_BUTTON_LEFT) {
		m_isPressed = false;
		_m_release.disconnect();
		_m_motion.disconnect();
	}
}

void ScrollBar::OnRawMouseMotion(MouseMotionEvent *e)
{
	if (m_isPressed) {
		float pos[2];
		GetAbsolutePosition(pos);
		float size[2];
		GetSize(size);
		if (m_isHoriz) {
			m_adjustment->SetValue((e->x-pos[0]) / float(size[0]));
		} else {
			m_adjustment->SetValue((e->y-pos[1]) / float(size[1]));
		}
	}
}

void ScrollBar::Draw()
{
	PROFILE_SCOPED()
	vector2f size; GetSize(size);
	if( !m_prevSize.ExactlyEqual(size) ) {
		m_prevSize = size;
		Theme::GenerateIndent(m_indent, size);
	}
	Theme::DrawIndent(m_indent, Screen::alphaBlendState);
	const float pos = m_adjustment->GetValue();
	if( !m_line.Valid() || pos != m_prevPos ) {
		SetupVertexBuffer(size, pos);
	}
	Screen::GetRenderer()->DrawBuffer(m_line.Get(), Screen::alphaBlendState, m_lineMaterial.Get(), Graphics::LINE_SINGLE);
}

void ScrollBar::SetupVertexBuffer(const vector2f &size, const float pos)
{
	Graphics::Renderer *r = Screen::GetRenderer();

	Graphics::VertexArray vertices(Graphics::ATTRIB_POSITION);
	vector3f lines[2];
	if (m_isHoriz) {
		vertices.Add(vector3f(BORDER+(size.x-2*BORDER)*pos, BORDER, 0.f));
		vertices.Add(vector3f(BORDER+(size.x-2*BORDER)*pos, size.y-BORDER, 0.f));
	} else {
		vertices.Add(vector3f(BORDER, BORDER+(size.y-2*BORDER)*pos, 0.f));
		vertices.Add(vector3f(size.x-BORDER, BORDER+(size.y-2*BORDER)*pos, 0.f));
	}

	struct LineVertex {
		vector3f pos;
	};

	Graphics::MaterialDescriptor desc;
	m_lineMaterial.Reset(r->CreateMaterial(desc));
	m_lineMaterial->diffuse = Color::WHITE;

	//Create vtx & index buffers and copy data
	Graphics::VertexBufferDesc vbd;
	vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
	vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
	vbd.attrib[0].offset   = offsetof(LineVertex, pos);
	vbd.stride = sizeof(LineVertex);
	vbd.numVertices = vertices.GetNumVerts();
	vbd.usage = Graphics::BUFFER_USAGE_STATIC;
	m_lineMaterial->SetupVertexBufferDesc( vbd );
	m_line.Reset(r->CreateVertexBuffer(vbd));
	LineVertex* vtxPtr = m_line->Map<LineVertex>(Graphics::BUFFER_MAP_WRITE);
	assert(m_line->GetDesc().stride == sizeof(LineVertex));
	for(Uint32 i=0 ; i<vertices.GetNumVerts() ; i++)
	{
		vtxPtr[i].pos	= vertices.position[i];
	}
	m_line->Unmap();
}

void ScrollBar::GetSizeRequested(float size[2])
{
	if (m_isHoriz) {
		// full X size, minimal Y size
		size[1] = SCROLLBAR_SIZE;
	} else {
		// full Y size, minimal X size
		size[0] = SCROLLBAR_SIZE;
	}
}

void ScrollBar::GetMinimumSize(float size[2])
{
	// who knows what the minimum size size is. odds are good that we're next
	// to a VScrollPortal which will provide a sane minimum size and the
	// container will sort out the rest
	size[0] = size[1] = SCROLLBAR_SIZE;
}

}
