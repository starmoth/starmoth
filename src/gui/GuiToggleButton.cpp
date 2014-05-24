// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "libs.h"
#include "Gui.h"

static const float BUTTON_SIZE = 16.f;

namespace Gui {
ToggleButton::ToggleButton()
{
	m_pressed = false;
	SetSize(BUTTON_SIZE, BUTTON_SIZE);
}
bool ToggleButton::OnMouseDown(MouseButtonEvent *e)
{
	if (e->button == SDL_BUTTON_LEFT) {
		onPress.emit();
		m_pressed = !m_pressed;
		if (m_pressed) {
			onChange.emit(this, true);
		} else {
			onChange.emit(this, false);
		}
	}
	return false;
}

void ToggleButton::OnActivate()
{
	m_pressed = !m_pressed;
	if (m_pressed) {
		onChange.emit(this, true);
	} else {
		onChange.emit(this, false);
	}
}

void ToggleButton::GetSizeRequested(float size[2])
{
	size[0] = BUTTON_SIZE;
	size[1] = BUTTON_SIZE;
}

void ToggleButton::Draw()
{
	PROFILE_SCOPED()
	vector2f size; GetSize(size);
	if( !m_prevSize.ExactlyEqual(size) ) {
		Theme::GenerateIndent(m_indent, m_prevSize);
		Theme::GenerateOutdent(m_outdent, m_prevSize);
		m_prevSize = size;
	}
	
	if (m_pressed) {
		Theme::DrawIndent(m_indent, Screen::alphaBlendState);
	} else {
		Theme::DrawOutdent(m_outdent, Screen::alphaBlendState);
	}
}

}
