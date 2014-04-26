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
	m_program->invLogZfarPlus1.Set(m_renderer->m_invLogZfarPlus1);
}

void Material::Unapply()
{
}

}
}
