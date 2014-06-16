// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Gui.h"

#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"

namespace Gui {

LabelSet::LabelSet() : Widget()
{
	m_eventMask = EVENT_MOUSEDOWN;
	m_labelsVisible = true;
	m_labelsClickable = true;
	m_labelColor = Color::WHITE;
	m_font = Screen::GetFont();
}

bool LabelSet::OnMouseDown(Gui::MouseButtonEvent *e)
{
	if ((e->button == SDL_BUTTON_LEFT) && (m_labelsClickable)) {
		for (auto i : m_items) {
			if ((fabs(e->x - i.screenx) < 10.0f) &&
			    (fabs(e->y - i.screeny) < 10.0f)) {
				i.onClick();
				return false;
			}
		}
	}
	return true;
}

bool LabelSet::CanPutItem(float x, float y)
{
	for (auto i : m_items) {
		if ((fabs(x-i.screenx) < 5.0f) &&
		    (fabs(y-i.screeny) < 5.0f)) return false;
	}
	return true;
}

void LabelSet::Add(std::string text, sigc::slot<void> onClick, float screenx, float screeny)
{
	if (CanPutItem(screenx, screeny)) {
		m_items.push_back(LabelSetItem(text, onClick, screenx, screeny));
	}
}

void LabelSet::Add(std::string text, sigc::slot<void> onClick, float screenx, float screeny, const Color &col)
{
	if (CanPutItem(screenx, screeny)) {
		m_items.push_back(LabelSetItem(text, onClick, screenx, screeny, col));
	}
}

void LabelSet::Clear()
{
	m_items.clear();
}

void LabelSet::Draw()
{
	PROFILE_SCOPED()
	if (!m_labelsVisible) return;
	
	Graphics::Renderer *r = Gui::Screen::GetRenderer();
	const matrix4x4f modelMatrix_ = r->GetCurrentModelView();

	const float scaleX = Screen::GetCoords2Pixels()[0];
	const float scaleY = Screen::GetCoords2Pixels()[1];
	const float halfFontHeight = Gui::Screen::GetFontHeight() * 0.5f;

	for (auto i : m_items) 
	{
		if( !i.m_vbuffer.Valid() )
		{
			Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE | Graphics::ATTRIB_UV0);
			m_font->PopulateString(va, i.text, 0, 0, i.hasOwnColor ? i.color : m_labelColor);
			i.m_vbuffer.Reset(m_font->CreateVertexBuffer(va));
		}

		{
			Graphics::Renderer::MatrixTicket ticket(r, Graphics::MatrixMode::MODELVIEW);

			const float x = modelMatrix_[12] + i.screenx;
			const float y = modelMatrix_[13] + i.screeny - halfFontHeight;

			r->LoadIdentity();
			r->Translate(floor(x/scaleX)*scaleX, floor(y/scaleY)*scaleY, 0);
			r->Scale(scaleX, scaleY, 1);

			m_font->RenderBuffer( i.m_vbuffer.Get(), i.hasOwnColor ? i.color : m_labelColor );
		}
	}
}

void LabelSet::GetSizeRequested(float size[2])
{
	size[0] = 800.0f;
	size[1] = 600.0f;
}

}
