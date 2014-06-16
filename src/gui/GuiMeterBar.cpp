// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Gui.h"

static const float METERBAR_PADDING    = 5.0f;
static const float METERBAR_BAR_HEIGHT = 8.0f;

namespace Gui {

MeterBar::MeterBar(float width, const char *label, const ::Color &graphCol) :
	m_prevLargeSize(0.0f),
	m_prevSmallSize(0.0f)
{
	m_requestedWidth = width;
	m_barValue = 0;
	m_barColor = graphCol;
	m_label = new Gui::Label(label);
	Add(m_label, METERBAR_PADDING, METERBAR_PADDING + METERBAR_BAR_HEIGHT);
	m_label->Show();
}

void MeterBar::Draw()
{
	PROFILE_SCOPED()
	vector2f size;
	GetSize(size);

	if( !m_prevLargeSize.ExactlyEqual(size) ) {
		m_largeVB.Reset( Theme::GenerateRoundEdgedRect(size, 5.0f) );
		m_prevLargeSize = size;
	}

	Graphics::Renderer *r = Gui::Screen::GetRenderer();

	Gui::Theme::DrawRoundEdgedRect(m_largeVB.Get(), Color(255,255,255,32), Screen::alphaBlendState);

	Graphics::Renderer::MatrixTicket ticket(r, Graphics::MatrixMode::MODELVIEW);

	r->Translate(METERBAR_PADDING, METERBAR_PADDING, 0.0f);
	size.x = m_barValue * (size.x - 2.0f*METERBAR_PADDING);
	size.y = METERBAR_BAR_HEIGHT;
	if( !m_prevSmallSize.ExactlyEqual(size) ) {
		m_smallVB.Reset( Theme::GenerateRoundEdgedRect(size, 3.0f) );
		m_prevSmallSize = size;
	}
	Gui::Theme::DrawRoundEdgedRect(m_smallVB.Get(), m_barColor, Screen::alphaBlendState);

	Gui::Fixed::Draw();
}

void MeterBar::GetSizeRequested(float size[2])
{
	size[0] = m_requestedWidth;
	size[1] = METERBAR_PADDING*2.0f + METERBAR_BAR_HEIGHT + Gui::Screen::GetFontHeight();
}

}
