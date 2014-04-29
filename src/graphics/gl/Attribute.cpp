// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Attribute.h"
#include "graphics/gl/glTexture.h"

namespace Graphics {
namespace PiGL {

Attribute::Attribute()
: m_location(-1)
{
}

void Attribute::Init(const char *name, GLuint program)
{
	m_location = glGetAttribLocation(program, name);
}

}
}
