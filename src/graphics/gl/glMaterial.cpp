// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "glMaterial.h"
#include "Program.h"
#include "glRenderer.h"

namespace Graphics {
namespace PiGL {

void Material::Apply()
{
	m_program->Use();

	// Attributes
	m_program->aVertex.Bind();
	m_program->aNormal.Bind();
	m_program->aColor.Bind();
	m_program->aUV0.Bind();

	m_program->invLogZfarPlus1.Set(m_renderer->m_invLogZfarPlus1);
}

void Material::Unapply()
{
}

}
}
