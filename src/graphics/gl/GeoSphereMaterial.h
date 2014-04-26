// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _PIGL_GEOSPHEREMATERIAL_H
#define _PIGL_GEOSPHEREMATEIRAL_H
/*
 * Programs & Materials used by terrain
 */
#include "libs.h"
#include "glMaterial.h"
#include "Program.h"
#include "galaxy/StarSystem.h"

namespace Graphics {
	namespace PiGL {
		class GeoSphereProgram : public Program {
		public:
			GeoSphereProgram(const std::string &filename, const std::string &defines);

			Uniform atmosColor;
			Uniform geosphereAtmosFogDensity;
			Uniform geosphereAtmosInvScaleHeight;
			Uniform geosphereAtmosTopRad; // in planet radii
			Uniform geosphereCenter;
			Uniform geosphereScale;
			Uniform geosphereScaledRadius; // (planet radius) / scale

			Uniform shadows;
			Uniform occultedLight;
			Uniform shadowCentreX;
			Uniform shadowCentreY;
			Uniform shadowCentreZ;
			Uniform srad;
			Uniform lrad;
			Uniform sdivlrad;

		protected:
			virtual void InitShaderLocations();
		};

		class GeoSphereSurfaceMaterial : public Material {
			virtual Program *CreateProgram(const MaterialDescriptor &);
			virtual void Apply();

		protected:
			void SetGSUniforms();
		};

		class GeoSphereSkyMaterial : public GeoSphereSurfaceMaterial {
		public:
			virtual Program *CreateProgram(const MaterialDescriptor &);
			virtual void Apply();
		};
	}
}
#endif
