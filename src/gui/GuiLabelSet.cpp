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
		for (std::vector<LabelSetItem>::iterator i = m_items.begin(); i != m_items.end(); ++i) {
			if ((fabs(e->x - (*i).screenx) < 10.0f) &&
			    (fabs(e->y - (*i).screeny) < 10.0f)) {
				(*i).onClick();
				return false;
			}
		}
	}
	return true;
}

bool LabelSet::CanPutItem(float x, float y)
{
	for (std::vector<LabelSetItem>::iterator i = m_items.begin(); i != m_items.end(); ++i) {
		if ((fabs(x-(*i).screenx) < 5.0f) &&
		    (fabs(y-(*i).screeny) < 5.0f)) return false;
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
	for (std::vector<LabelSetItem>::iterator i = m_items.begin(); i != m_items.end(); ++i) 
	{
		if( !(*i).m_vbuffer.Valid() )
		{
			Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE | Graphics::ATTRIB_UV0);
			m_font->PopulateString(va, (*i).text, (*i).screenx, (*i).screeny - Gui::Screen::GetFontHeight()*0.5f, (*i).hasOwnColor ? (*i).color : m_labelColor);
			(*i).m_vbuffer.Reset(m_font->CreateVertexBuffer(va));
		}
		m_font->RenderBuffer( (*i).m_vbuffer.Get(), (*i).hasOwnColor ? (*i).color : m_labelColor );
	}
}

void LabelSet::GetSizeRequested(float size[2])
{
	size[0] = 800.0f;
	size[1] = 600.0f;
}

}
