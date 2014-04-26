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
			}
		};
	}
}
#endif
