// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#ifndef _SLICE_H
#define _SLICE_H

#include <vector>
#include <utility>

namespace Slice {

	// Drive State
	enum class DriveState {
		DRIVE_OFF,
		DRIVE_READY,
		DRIVE_START,
		DRIVE_ON,
		DRIVE_STOP,
		DRIVE_FINISHED
	};

	// Slice parameters
	typedef std::pair<double, double> RangeSpeedPair;
	typedef std::vector<RangeSpeedPair> RSPVector;
	void BodyMinRanges(RSPVector &out);

	// quick accessors
	double MaxSliceSpeed();
	double MaxRangeSpeed();

	double EngageDriveMinSpeed();	// relative or absolute?
};

#endif /* _SLICE_H */


