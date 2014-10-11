// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "glMaterial.h"
#include "Program.h"
#include "glRenderer.h"
#include "graphics/VertexBuffer.h"
#include "glVertexBuffer.h"

namespace Graphics {
namespace PiGL {

void Material::Apply()
{
	m_program->Use();
	m_program->invLogZfarPlus1.Set(m_renderer->m_invLogZfarPlus1);
}

void Material::Unapply()
{
}

void Material::SetupVertexBufferDesc( VertexBufferDesc& vbd) const
{
	for (int i = 0; i < MAX_ATTRIBS; i++) {
		auto& attr  = vbd.attrib[i];
		if (attr.semantic == ATTRIB_NONE)
			break;

		switch (attr.semantic) {
		case ATTRIB_POSITION:	attr.location = m_program->aVertex.Location();	break;
		case ATTRIB_NORMAL:		attr.location = m_program->aNormal.Location();	break;
		case ATTRIB_DIFFUSE:	attr.location = m_program->aColor.Location();	break;
		case ATTRIB_UV0:		attr.location = m_program->aUV0.Location();		break;
		// bad things
		case ATTRIB_NONE:
		default:
			return;
		}
	}
}

}
}
