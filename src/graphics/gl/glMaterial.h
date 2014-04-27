// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _PIGL_MATERIAL_H
#define _PIGL_MATERIAL_H
/*
 * Multi-purpose PiGL material.
 *
 * Generally, the idea is that a Program contains uniforms but
 * a material sets them, using the standard parameters of Graphics::Material
 * or whatever necessary to achieve an effect
 *
 * Programs are owned by the renderer, since they are shared between materials.
 */
#include "libs.h"
#include "graphics/Material.h"

namespace Graphics {

	class glRenderer;

	namespace PiGL {

		class Program;

		class Material : public Graphics::Material {
		public:
			Material() { }
			// Create an appropriate program for this material.
			virtual Program *CreateProgram(const MaterialDescriptor &) = 0;
			// bind textures, set uniforms
			virtual void Apply();
			virtual void Unapply();
			virtual void SetProgram(Program *p) { m_program = p; }

		protected:
			friend class Graphics::glRenderer;
			Program *m_program;
			glRenderer *m_renderer;
		};
	}
}
#endif
