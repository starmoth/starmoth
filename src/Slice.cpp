// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Slice.h"

static const double SLICE_GRAVITY_RANGE_1 = 15000.0;
static const double SLICE_GRAVITY_RANGE_2 = 1000000.0;
static const double SLICE_START_SPEED = 50000.0;
static const double SLICE_DRIVE_1_SPEED = 299000.0;
static const double SLICE_DRIVE_2_SPEED = 2997924580.0; // lightspeed * 10 ???

namespace Slice {
	// Slice parameters
	void BodyMinRanges(RSPVector &out) {
		// largest values first please
		out.push_back( std::make_pair(SLICE_GRAVITY_RANGE_2, SLICE_DRIVE_2_SPEED) );
		out.push_back( std::make_pair(SLICE_GRAVITY_RANGE_1, SLICE_DRIVE_1_SPEED) );
	}
	
	// quick accessors
	double MaxSliceSpeed() { return SLICE_DRIVE_2_SPEED; }
	double MaxRangeSpeed() { return SLICE_GRAVITY_RANGE_2; }

	double EngageDriveMinSpeed() {	// relative or absolute?
		return SLICE_START_SPEED;
	}
};
