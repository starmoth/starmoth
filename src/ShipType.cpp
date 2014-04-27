// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ShipType.h"
#include "FileSystem.h"
#include "utils.h"
#include "json/json.h"

ShipType::ShipType(const std::string &_id, const std::string &path) {
	Json::Reader reader;
	Json::Value data;

	auto fd = FileSystem::gameDataFiles.ReadFile(path);
	if (!fd) {
		Output("couldn't open ship def '%s'\n", path.c_str());
		return;
	}

	if (!reader.parse(fd->GetData(), fd->GetData()+fd->GetSize(), data)) {
		Output("couldn't read ship def '%s': %s\n", path.c_str(), reader.getFormattedErrorMessages().c_str());
		return;
	}

	id = _id;
	name = data.get("name", "").asString();
	model = data.get("model", "").asString();
	cockpitModel = data.get("cockpit_model", "").asString();

	linThrust[THRUSTER_REVERSE] = data.get("reverse_thrust", 0.0f).asFloat();
	linThrust[THRUSTER_FORWARD] = data.get("forward_thrust", 0.0f).asFloat();
	linThrust[THRUSTER_UP] = data.get("up_thrust", 0.0f).asFloat();
	linThrust[THRUSTER_DOWN] = data.get("down_thrust", 0.0f).asFloat();
	linThrust[THRUSTER_LEFT] = data.get("left_thrust", 0.0f).asFloat();
	linThrust[THRUSTER_RIGHT] = data.get("right_thrust", 0.0f).asFloat();

	angThrust = data.get("angular_thrust", 0.0f).asFloat();
	hullMass = data.get("hull_mass", 0).asFloat();
	effectiveExhaustVelocity = data.get("effective_exhaust_velocity", 0.0f).asFloat();
}

std::map<const std::string, const ShipType> ShipType::types;

void ShipType::Init()
{
	static bool isInitted = false;
	if (isInitted) return;
	isInitted = true;

	// load all ship definitions
	namespace fs = FileSystem;
	for (fs::FileEnumerator files(fs::gameDataFiles, "ships", 0); !files.Finished(); files.Next()) {
		const fs::FileInfo &info = files.Current();
		if (ends_with_ci(info.GetPath(), ".json")) {
			const std::string id(info.GetName().substr(0, info.GetName().size()-4));
			ShipType st = ShipType(id, info.GetPath());
			types.insert(std::make_pair(st.id, st));
		}
	}
}
