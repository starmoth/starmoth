// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt
#ifndef _GL2_SPHEREIMPOSTORMATERIAL_H
#define _GL2_SPHEREIMPOSTORMATERIAL_H
/*
 * Billboard sphere impostor
 */
#include "libs.h"
#include "glMaterial.h"
#include "Program.h"
namespace Graphics {

	namespace PiGL {

		class SphereImpostorMaterial : public Material {
		public:
			Program *CreateProgram(const MaterialDescriptor &) {
				return new Program("billboard_sphereimpostor", "");
			}

			virtual void Apply() override {
				PiGL::Material::Apply();
				m_program->sceneAmbient.Set(m_renderer->GetAmbientColor());

				//Light uniform parameters
				for( int i=0 ; i<m_renderer->GetNumLights() ; i++ ) {
					const Light& Light = m_renderer->GetLight(i);
					m_program->lights[i].diffuse.Set( Light.GetDiffuse() );
					m_program->lights[i].specular.Set( Light.GetSpecular() );
					m_program->lights[i].position.Set( Light.GetPosition().x, Light.GetPosition().y, Light.GetPosition().z, (Light.GetType() == Light::LIGHT_DIRECTIONAL ? 0.f : 1.f));
				}
			}
		};
	}
}
#endif
