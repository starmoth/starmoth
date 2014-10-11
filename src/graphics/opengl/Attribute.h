// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _PIGL_ATTRIBUTE_H
#define _PIGL_ATTRIBUTE_H
/*
 * Shader uniform
 */
#include "libs.h"
namespace Graphics {

	class Texture;

	namespace PiGL {
		class Attribute {
		public:
			Attribute();
			void Init(const char *name, GLuint program);

			GLint Location() const { return m_location; }

		private:
			GLint m_location;
		};
	}
}

#endif
