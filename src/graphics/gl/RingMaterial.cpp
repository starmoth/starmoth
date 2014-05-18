// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "RingMaterial.h"
#include "StringF.h"
#include "graphics/Graphics.h"
#include "graphics/Light.h"
#include "glRenderer.h"
#include "graphics/gl/glTexture.h"

namespace Graphics {
namespace PiGL {

Program *RingMaterial::CreateProgram(const MaterialDescriptor &desc)
{
	assert(desc.textures == 1);
	//pick light count and some defines
	unsigned int numLights = Clamp(desc.dirLights, 1u, 4u);
	std::string defines = stringf("#define NUM_LIGHTS %0{u}\n", numLights);
	return new Program("planetrings", defines);
}

void RingMaterial::Apply()
{
	PiGL::Material::Apply();

	assert(this->texture0);
	static_cast<TextureGL*>(texture0)->Bind();
	m_program->texture0.Set(0);

	//Light uniform parameters
	for( int i=0 ; i<m_renderer->GetNumLights() ; i++ ) {
		const Light& Light = m_renderer->GetLight(i);
		m_program->lights[i].diffuse.Set( Light.GetDiffuse() );
		m_program->lights[i].specular.Set( Light.GetSpecular() );
		m_program->lights[i].position.Set( Light.GetPosition().x, Light.GetPosition().y, Light.GetPosition().z, (Light.GetType() == Light::LIGHT_DIRECTIONAL ? 0.f : 1.f));
	}
}

void RingMaterial::Unapply()
{
	static_cast<TextureGL*>(texture0)->Unbind();
}

}
}
