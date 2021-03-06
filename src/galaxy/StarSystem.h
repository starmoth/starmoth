// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _STARSYSTEM_H
#define _STARSYSTEM_H

#include "libs.h"
#include "Serializer.h"
#include <vector>
#include <string>
#include "RefCounted.h"
#include "galaxy/SystemPath.h"
#include "galaxy/GalaxyCache.h"
#include "Orbit.h"
#include "IterationProxy.h"
#include "gameconsts.h"
#include <SDL_stdinc.h>

class SystemBody;

// doubles - all masses in Kg, all lengths in meters
// fixed - any mad scheme

class StarSystem;
class Faction;

struct RingStyle {
	// note: radius values are given as proportions of the planet radius
	// (e.g., 1.6)
	fixed minRadius;
	fixed maxRadius;
	Color baseColor;
};

class SystemBody : public RefCounted {
public:
	SystemBody(const SystemPath& path);
	void PickPlanetType(Random &rand);
	const SystemBody *FindStarAndTrueOrbitalRange(fixed &orbMin, fixed &orbMax);

	enum BodyType { // <enum scope='SystemBody' prefix=TYPE_ public>
		TYPE_GRAVPOINT = 0,
		TYPE_BROWN_DWARF = 1, //  L+T Class Brown Dwarfs
		TYPE_WHITE_DWARF = 2,
		TYPE_STAR_M = 3, //red
		TYPE_STAR_K = 4, //orange
		TYPE_STAR_G = 5, //yellow
		TYPE_STAR_F = 6, //white
		TYPE_STAR_A = 7, //blue/white
		TYPE_STAR_B = 8, //blue
		TYPE_STAR_O = 9,  //blue/purple/white
		TYPE_STAR_M_GIANT = 10,
		TYPE_STAR_K_GIANT = 11,
		TYPE_STAR_G_GIANT = 12,
		TYPE_STAR_F_GIANT = 13,
		TYPE_STAR_A_GIANT = 14,
		TYPE_STAR_B_GIANT = 15,
		TYPE_STAR_O_GIANT = 16,
		TYPE_STAR_M_SUPER_GIANT = 17,
		TYPE_STAR_K_SUPER_GIANT = 18,
		TYPE_STAR_G_SUPER_GIANT = 19,
		TYPE_STAR_F_SUPER_GIANT = 20,
		TYPE_STAR_A_SUPER_GIANT = 21,
		TYPE_STAR_B_SUPER_GIANT = 22,
		TYPE_STAR_O_SUPER_GIANT = 23,
		TYPE_STAR_M_HYPER_GIANT = 24,
		TYPE_STAR_K_HYPER_GIANT = 25,
		TYPE_STAR_G_HYPER_GIANT = 26,
		TYPE_STAR_F_HYPER_GIANT = 27,
		TYPE_STAR_A_HYPER_GIANT = 28,
		TYPE_STAR_B_HYPER_GIANT = 29,
		TYPE_STAR_O_HYPER_GIANT = 30, // these various stars do exist = they are transitional states and are rare
		TYPE_STAR_M_WF = 31,  //Wolf-Rayet star
		TYPE_STAR_B_WF = 32,  // while you do not specifically get class M,B or O WF stars,
		TYPE_STAR_O_WF = 33, //  you do get red = blue and purple from the colour of the gasses = so spectral class is an easy way to define them.
		TYPE_STAR_S_BH = 34, //stellar blackhole
		TYPE_STAR_IM_BH = 35, //Intermediate-mass blackhole
		TYPE_STAR_SM_BH = 36, //Supermassive blackhole
		TYPE_PLANET_GAS_GIANT = 37,
		TYPE_PLANET_ASTEROID = 38,
		TYPE_PLANET_TERRESTRIAL = 39,
		TYPE_STARPORT_ORBITAL = 40,
		TYPE_STARPORT_SURFACE = 41,
		TYPE_MIN = TYPE_BROWN_DWARF, // <enum skip>
		TYPE_MAX = TYPE_STARPORT_SURFACE, // <enum skip>
		TYPE_STAR_MIN = TYPE_BROWN_DWARF, // <enum skip>
		TYPE_STAR_MAX = TYPE_STAR_SM_BH, // <enum skip>
		// XXX need larger atmosphereless thing
	};

	enum BodySuperType { // <enum scope='SystemBody' prefix=SUPERTYPE_ public>
		SUPERTYPE_NONE = 0,
		SUPERTYPE_STAR = 1,
		SUPERTYPE_ROCKY_PLANET = 2,
		SUPERTYPE_GAS_GIANT = 3,
		SUPERTYPE_STARPORT = 4,
	};

	const SystemPath& GetPath() const { return m_path; }
	SystemBody* GetParent() const { return m_parent; }

	bool HasChildren() const { return !m_children.empty(); }
	unsigned GetNumChildren() const { return m_children.size(); }
	IterationProxy<std::vector<SystemBody*> > GetChildren() { return MakeIterationProxy(m_children); }
	const IterationProxy<const std::vector<SystemBody*> > GetChildren() const { return MakeIterationProxy(m_children); }

	std::string GetName() const { return m_name; }
	std::string GetAstroDescription() const;
	const char *GetIcon() const;
	BodyType GetType() const { return m_type; }
	BodySuperType GetSuperType() const;
	fixed GetRadiusAsFixed() const { return m_radius; }
	double GetRadius() const { // polar radius
		if (GetSuperType() <= SUPERTYPE_STAR)
			return (m_radius.ToDouble() / m_aspectRatio.ToDouble()) * SOL_RADIUS;
		else
			return m_radius.ToDouble() * EARTH_RADIUS;
	}
	double GetAspectRatio() const { return m_aspectRatio.ToDouble(); }
	fixed GetMassAsFixed() const { return m_mass; }
	double GetMass() const {
		if (GetSuperType() <= SUPERTYPE_STAR)
			return m_mass.ToDouble() * SOL_MASS;
		else
			return m_mass.ToDouble() * EARTH_MASS;
	}
	fixed GetMassInEarths() const {
		if (GetSuperType() <= SUPERTYPE_STAR)
			return m_mass * 332998;
		else
			return m_mass;
	}
	bool IsRotating() const { return m_rotationPeriod != fixed(0); }
	// returned in seconds
	double GetRotationPeriodInDays() const { return m_rotationPeriod.ToDouble(); }
	double GetRotationPeriod() const {
		return m_rotationPeriod.ToDouble()*60*60*24;
	}
	bool HasRotationPhase() const { return m_rotationalPhaseAtStart != fixed(0); }
	double GetRotationPhaseAtStart() const { return m_rotationalPhaseAtStart.ToDouble(); }
	double GetAxialTilt() const { return m_axialTilt.ToDouble(); }

	const Orbit& GetOrbit() const { return m_orbit; }
	double GetEccentricity() const { return m_eccentricity.ToDouble(); }
	double GetOrbMin() const { return m_orbMin.ToDouble(); }
	double GetOrbMax() const { return m_orbMax.ToDouble(); }
	double GetSemiMajorAxis() const { return m_semiMajorAxis.ToDouble(); }
	void SetOrbitPlane(const matrix3x3d &orient) { m_orbit.SetPlane(orient); }

	int GetAverageTemp() const { return m_averageTemp; }
	std::string GetHeightMapFilename() const { return m_heightMapFilename; }
	unsigned int GetHeightMapFractal() const { return m_heightMapFractal; }

	Uint32 GetSeed() const { return m_seed; }

	fixed GetMetallicity() const { return m_metallicity; }
	fixed GetVolatileGas() const { return m_volatileGas; }
	fixed GetVolatileLiquid() const { return m_volatileLiquid; }
	fixed GetVolatileIces() const { return m_volatileIces; }
	fixed GetVolcanicity() const { return m_volcanicity; }
	fixed GetAtmosOxidizing() const { return m_atmosOxidizing; }
	fixed GetLife() const { return m_life; }

	fixed CalcHillRadius() const;
	static int CalcSurfaceTemp(const SystemBody *primary, fixed distToPrimary, fixed albedo, fixed greenhouse);
	double CalcSurfaceGravity() const;

	double GetMaxChildOrbitalDistance() const;
	void PositionSettlementOnPlanet();
	void PopulateStage1(StarSystem *system);
	void PopulateAddStations(StarSystem *system);

	bool HasRings() const { return bool(m_rings.maxRadius.v); }
	const RingStyle& GetRings() const { return m_rings; }
	void PickRings(bool forceRings = false);


	// XXX merge all this atmosphere stuff
	bool HasAtmosphere() const;

	Color GetAlbedo() const {
		// XXX suggestions about how to determine a sensible albedo colour would be welcome
		// Currently (2014-03-24) this is just used as the colour for the body billboard
		// which is rendered when the body has a small screen size
		return Color(200,200,200,255);
	}

	void PickAtmosphere();
	void GetAtmosphereFlavor(Color *outColor, double *outDensity) const {
		*outColor = m_atmosColor;
		*outDensity = m_atmosDensity;
	}

	struct AtmosphereParameters {
		float atmosRadius;
		float atmosInvScaleHeight;
		float atmosDensity;
		float planetRadius;
		Color atmosCol;
		vector3d center;
		float scale;
	};

	AtmosphereParameters CalcAtmosphereParams() const;

	bool IsScoopable() const;

	void Dump(FILE* file, const char* indent = "") const;

private:
	friend class StarSystem;
	friend class ObjectViewerView;

	void ClearParentAndChildPointers();

	SystemBody *m_parent;                // these are only valid if the StarSystem
	std::vector<SystemBody*> m_children; // that create them still exists

	SystemPath m_path;
	Orbit m_orbit;
	Uint32 m_seed; // Planet.cpp can use to generate terrain
	std::string m_name;
	fixed m_radius; // in earth radii for planets, sol radii for stars. equatorial radius in case of bodies which are flattened at the poles
	fixed m_aspectRatio; // ratio between equatorial and polar radius for bodies with eqatorial bulges
	fixed m_mass; // earth masses if planet, solar masses if star
	fixed m_orbMin, m_orbMax; // periapsism, apoapsis in AUs
	fixed m_rotationPeriod; // in days
	fixed m_rotationalPhaseAtStart; // 0 to 2 pi
	fixed m_humanActivity; // 0 - 1
	fixed m_semiMajorAxis; // in AUs
	fixed m_eccentricity;
	fixed m_orbitalOffset;
	fixed m_orbitalPhaseAtStart; // 0 to 2 pi
	fixed m_axialTilt; // in radians
	fixed m_inclination; // in radians, for surface bodies = latitude
	int m_averageTemp;
	BodyType m_type;

	/* composition */
	fixed m_metallicity; // (crust) 0.0 = light (Al, SiO2, etc), 1.0 = heavy (Fe, heavy metals)
	fixed m_volatileGas; // 1.0 = earth atmosphere density
	fixed m_volatileLiquid; // 1.0 = 100% ocean cover (earth = 70%)
	fixed m_volatileIces; // 1.0 = 100% ice cover (earth = 3%)
	fixed m_volcanicity; // 0 = none, 1.0 = fucking volcanic
	fixed m_atmosOxidizing; // 0.0 = reducing (H2, NH3, etc), 1.0 = oxidising (CO2, O2, etc)
	fixed m_life; // 0.0 = dead, 1.0 = teeming

	RingStyle m_rings;

	std::string m_heightMapFilename;
	unsigned int m_heightMapFractal;

	Color m_atmosColor;
	double m_atmosDensity;
};

class StarSystem : public RefCounted {
public:
	friend class SystemBody;
	friend class GalaxyObjectCache<StarSystem, SystemPath::LessSystemOnly>;

	static StarSystemCache attic;
	static RefCountedPtr<StarSystemCache::Slave> cache;

	const std::string &GetName() const { return m_name; }
	SystemPath GetPathOf(const SystemBody *sbody) const;
	SystemBody *GetBodyByPath(const SystemPath &path) const;
	static void Serialize(Serializer::Writer &wr, StarSystem *);
	static RefCountedPtr<StarSystem> Unserialize(Serializer::Reader &rd);
	const SystemPath &GetSystemPath() const { return m_path; }
	const char *GetLongDescription() const { return m_longDesc.c_str(); }
	int GetNumStars() const { return m_numStars; }

	static const Uint8 starColors[][3];
	static const Uint8 starRealColors[][3];
	static const double starLuminosities[];
	static const float starScale[];
	static const fixed starMetallicities[];

	RefCountedPtr<const SystemBody> GetRootBody() const { return m_rootBody; }
	RefCountedPtr<SystemBody> GetRootBody() { return m_rootBody; }
	bool HasSpaceStations() const { return !m_spaceStations.empty(); }
	unsigned GetNumSpaceStations() const { return m_spaceStations.size(); }
	IterationProxy<std::vector<SystemBody*> > GetSpaceStations() { return MakeIterationProxy(m_spaceStations); }
	const IterationProxy<const std::vector<SystemBody*> > GetSpaceStations() const { return MakeIterationProxy(m_spaceStations); }
	IterationProxy<std::vector<SystemBody*> > GetStars() { return MakeIterationProxy(m_stars); }
	const IterationProxy<const std::vector<SystemBody*> > GetStars() const { return MakeIterationProxy(m_stars); }
	unsigned GetNumBodies() const { return m_bodies.size(); }
	IterationProxy<std::vector<RefCountedPtr<SystemBody> > > GetBodies() { return MakeIterationProxy(m_bodies); }
	const IterationProxy<const std::vector<RefCountedPtr<SystemBody> > > GetBodies() const { return MakeIterationProxy(m_bodies); }

	Faction* GetFaction() const  { return m_faction; }
	bool GetUnexplored() const { return m_unexplored; }
	fixed GetMetallicity() const { return m_metallicity; }
	int GetSeed() const { return m_seed; }

	void Dump(FILE* file, const char* indent = "", bool suppressSectorData = false) const;

private:
	StarSystem(const SystemPath &path, StarSystemCache* cache);
	~StarSystem();

	void SetCache(StarSystemCache* cache) { assert(!m_cache); m_cache = cache; }

	SystemBody *NewBody() {
		SystemBody *body = new SystemBody(SystemPath(m_path.sectorX, m_path.sectorY, m_path.sectorZ, m_path.systemIndex, m_bodies.size()));
		m_bodies.push_back(RefCountedPtr<SystemBody>(body));
		return body;
	}
	void MakePlanetsAround(SystemBody *primary, Random &rand);
	void MakeRandomStar(SystemBody *sbody, Random &rand);
	void MakeStarOfType(SystemBody *sbody, SystemBody::BodyType type, Random &rand);
	void MakeStarOfTypeLighterThan(SystemBody *sbody, SystemBody::BodyType type, fixed maxMass, Random &rand);
	void MakeBinaryPair(SystemBody *a, SystemBody *b, fixed minDist, Random &rand);
	void Populate(bool addSpaceStations);
	std::string GetStarTypes(SystemBody *body);

	SystemPath m_path;
	int m_numStars;
	std::string m_name;
	std::string m_longDesc;

	Faction* m_faction;
	bool m_unexplored;
	fixed m_metallicity;
	int m_seed;

	RefCountedPtr<SystemBody> m_rootBody;
	// index into this will be the SystemBody ID used by SystemPath
	std::vector< RefCountedPtr<SystemBody> > m_bodies;
	std::vector<SystemBody*> m_spaceStations;
	std::vector<SystemBody*> m_stars;

	StarSystemCache* m_cache;
};

#endif /* _STARSYSTEM_H */
