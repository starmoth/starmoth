// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SHIPTYPE_H
#define _SHIPTYPE_H

#include "libs.h"
#include "vector3.h"
#include <vector>
#include <map>

class ShipType {
public:
	enum Thruster { // <enum scope='ShipType' name=ShipTypeThruster prefix=THRUSTER_ public>
		THRUSTER_REVERSE,
		THRUSTER_FORWARD,
		THRUSTER_UP,
		THRUSTER_DOWN,
		THRUSTER_LEFT,
		THRUSTER_RIGHT,
		THRUSTER_MAX // <enum skip>
	};

	ShipType(const std::string &id, const std::string &path);

	std::string id;
	std::string name;
	std::string model;
	std::string cockpitModel;
	float linThrust[THRUSTER_MAX];
	float angThrust;
	float hullMass;
	float effectiveExhaustVelocity; // velocity at which the propellant escapes the engines

	static std::map<const std::string, const ShipType> types;

	static void Init();
};

#endif /* _SHIPTYPE_H */
