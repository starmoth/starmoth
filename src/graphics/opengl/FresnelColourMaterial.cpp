// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "FresnelColourMaterial.h"
#include "graphics/Material.h"
#include "graphics/opengl/glTexture.h"
#include "graphics/Graphics.h"
#include "glRenderer.h"
#include <sstream>
#include "StringF.h"

namespace Graphics {
namespace PiGL {

FresnelColourProgram::FresnelColourProgram(const MaterialDescriptor &desc, int lights)
{
	//build some defines
	std::stringstream ss;

	m_name = "FresnelColour";
	m_defines = ss.str();

	LoadShaders(m_name, m_defines);
	InitShaderLocations();
}

Program *FresnelColourMaterial::CreateProgram(const MaterialDescriptor &desc)
{
	return new FresnelColourProgram(desc);
}

void FresnelColourMaterial::Apply()
{
	PiGL::Material::Apply();
	FresnelColourProgram *p = static_cast<FresnelColourProgram*>(m_program);
	p->diffuse.Set(this->diffuse);
}

}
}
