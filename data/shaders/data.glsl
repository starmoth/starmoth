// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

uniform mat4 uProjectionMatrix;
uniform mat4 uViewMatrix;
uniform mat4 uViewMatrixInverse;
uniform mat4 uViewProjectionMatrix;
uniform mat3 uNormalMatrix;

//Light uniform parameters
struct Light {
	vec4 diffuse;
	vec4 specular;
	vec3 position;
};
uniform Light uLight[4];

struct MaterialParameters {
	vec4 emission;
	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
	float shininess;
};

in vec4 a_vertex;
in vec3 a_normal;
in vec4 a_color;
in vec2 a_uv0;

// nVidia complains if this isn't set even though I'm just using the regular "textureCube" sampler.
#extension GL_NV_shadow_samplers_cube : enable