// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

uniform sampler2D texture0;

out vec4 frag_color;

void main(void)
{
	// Bits of ring in shadow!
	vec4 col = vec4(0.0);
	vec4 texCol = texture2D(texture0, gl_TexCoord[0].st);

	for (int i=0; i<NUM_LIGHTS; ++i) {
		float l = findSphereEyeRayEntryDistance(-vec3(gl_TexCoord[1]), vec3(uViewMatrixInverse * uLight[i].position), 1.0);
		if (l <= 0.0) {
			col = col + texCol*uLight[i].diffuse;
		}
	}
	col.a = texCol.a;
	frag_color = col;

	SetFragDepth();
}