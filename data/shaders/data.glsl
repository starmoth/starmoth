// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uViewMatrixInverse;
uniform mat4 uViewProjectionMatrix;
uniform mat3 uNormalMatrix;

//Light uniform parameters
struct Light {
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	vec4 position;
};
uniform Light uLightSource[4];

in vec4 a_vertex;
in vec3 a_normal;
in vec4 a_color;
in vec2 a_uv0;
