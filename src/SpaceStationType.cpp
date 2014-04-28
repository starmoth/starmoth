// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "SpaceStationType.h"
#include "FileSystem.h"
#include "Pi.h"
#include "MathUtil.h"
#include "Ship.h"
#include "StringF.h"
#include "scenegraph/Model.h"
#include "OS.h"
#include "json/json.h"

#include <algorithm>

//static lua_State *s_lua;
static std::string s_currentStationFile = "";

const SpaceStationType::SBayGroup* SpaceStationType::FindGroupByBay(const int zeroBaseBayID) const
{
	for (TBayGroups::const_iterator bayIter = bayGroups.begin(), grpEnd=bayGroups.end(); bayIter!=grpEnd ; ++bayIter ) {
		for (std::vector<int>::const_iterator idIter = (*bayIter).bayIDs.begin(), idIEnd = (*bayIter).bayIDs.end(); idIter!=idIEnd ; ++idIter ) {
			if ((*idIter)==zeroBaseBayID) {
				return &(*bayIter);
			}
		}
	}
	// is it safer to return that the bay is locked?
	return 0;
}

SpaceStationType::SBayGroup* SpaceStationType::GetGroupByBay(const int zeroBaseBayID)
{
	for (TBayGroups::iterator bayIter = bayGroups.begin(), grpEnd=bayGroups.end(); bayIter!=grpEnd ; ++bayIter ) {
		for (std::vector<int>::const_iterator idIter = (*bayIter).bayIDs.begin(), idIEnd = (*bayIter).bayIDs.end(); idIter!=idIEnd ; ++idIter ) {
			if ((*idIter)==zeroBaseBayID) {
				return &(*bayIter);
			}
		}
	}
	// is it safer to return that the bay is locked?
	return 0;
}

bool SpaceStationType::GetShipApproachWaypoints(const unsigned int port, const int stage, positionOrient_t &outPosOrient) const
{
	bool gotOrient = false;

	const SBayGroup* pGroup = FindGroupByBay(port);
	if (pGroup && stage>0) {
		TMapBayIDMat::const_iterator stageDataIt = pGroup->m_approach.find(stage);
		if (stageDataIt != pGroup->m_approach.end()) {
			const matrix4x4f &mt = pGroup->m_approach.at(stage);
			outPosOrient.pos	= vector3d(mt.GetTranslate());
			outPosOrient.xaxis	= vector3d(mt.GetOrient().VectorX());
			outPosOrient.yaxis	= vector3d(mt.GetOrient().VectorY());
			outPosOrient.zaxis	= vector3d(mt.GetOrient().VectorZ());
			outPosOrient.xaxis	= outPosOrient.xaxis.Normalized();
			outPosOrient.yaxis	= outPosOrient.yaxis.Normalized();
			outPosOrient.zaxis	= outPosOrient.zaxis.Normalized();
			gotOrient = true;
		}
	}
	return gotOrient;
}

double SpaceStationType::GetDockAnimStageDuration(const int stage) const
{
	assert(stage>=0 && stage<numDockingStages);
	return dockAnimStageDuration[stage];
}

double SpaceStationType::GetUndockAnimStageDuration(const int stage) const
{
	assert(stage>=0 && stage<numUndockStages);
	return undockAnimStageDuration[stage];
}

static bool GetPosOrient(const SpaceStationType::TMapBayIDMat &bayMap, const int stage, const double t, const vector3d &from,
				  SpaceStationType::positionOrient_t &outPosOrient)
{
	bool gotOrient = false;

	vector3d toPos;

	const SpaceStationType::TMapBayIDMat::const_iterator stageDataIt = bayMap.find( stage );
	const bool bHasStageData = (stageDataIt != bayMap.end());
	assert(bHasStageData);
	if (bHasStageData) {
		const matrix4x4f &mt = stageDataIt->second;
		outPosOrient.xaxis	= vector3d(mt.GetOrient().VectorX()).Normalized();
		outPosOrient.yaxis	= vector3d(mt.GetOrient().VectorY()).Normalized();
		outPosOrient.zaxis	= vector3d(mt.GetOrient().VectorZ()).Normalized();
		toPos				= vector3d(mt.GetTranslate());
		gotOrient = true;
	}

	if (gotOrient)
	{
		vector3d pos		= MathUtil::mix<vector3d, double>(from, toPos, t);
		outPosOrient.pos	= pos;
	}

	return gotOrient;
}

/* when ship is on rails it returns true and fills outPosOrient.
 * when ship has been released (or docked) it returns false.
 * Note station animations may continue for any number of stages after
 * ship has been released and is under player control again */
bool SpaceStationType::GetDockAnimPositionOrient(const unsigned int port, int stage, double t, const vector3d &from, positionOrient_t &outPosOrient, const Ship *ship) const
{
	assert(ship);
	if (stage < -shipLaunchStage) { stage = -shipLaunchStage; t = 1.0; }
	if (stage > numDockingStages || !stage) { stage = numDockingStages; t = 1.0; }
	// note case for stageless launch (shipLaunchStage==0)

	bool gotOrient = false;

	assert(port<=m_ports.size());
	const Port &rPort = m_ports.at(port+1);
	if (stage<0) {
		const int leavingStage = (-1*stage);
		gotOrient = GetPosOrient(rPort.m_leaving, leavingStage, t, from, outPosOrient);
		const vector3d up = outPosOrient.yaxis.Normalized() * ship->GetLandingPosOffset();
		outPosOrient.pos = outPosOrient.pos - up;
	} else if (stage>0) {
		gotOrient = GetPosOrient(rPort.m_docking, stage, t, from, outPosOrient);
		const vector3d up = outPosOrient.yaxis.Normalized() * ship->GetLandingPosOffset();
		outPosOrient.pos = outPosOrient.pos - up;
	}

	return gotOrient;
}

SpaceStationType::SpaceStationType(const std::string &_id, const std::string &path) {
	Json::Reader reader;
	Json::Value data;

	auto fd = FileSystem::gameDataFiles.ReadFile(path);
	if (!fd) {
		Output("couldn't open station def '%s'\n", path.c_str());
		return;
	}

	if (!reader.parse(fd->GetData(), fd->GetData()+fd->GetSize(), data)) {
		Output("couldn't read station def '%s': %s\n", path.c_str(), reader.getFormattedErrorMessages().c_str());
		return;
	}

	id = _id;
	modelName = data.get("model", "").asString();

	const std::string type(data.get("type", "").asString());
	if (type == "surface")
		dockMethod = SURFACE;
	else if (type == "orbital")
		dockMethod = ORBITAL;
	else {
		Output("couldn't parse station def '%s': unknown type '%s'\n", path.c_str(), type.c_str());
		return;
	}

	angVel = data.get("angular_velocity", 0.0f).asFloat();

	parkingDistance = data.get("parking_distance", 0.0f).asFloat();
	parkingGapSize = data.get("parking_gap_size", 0.0f).asFloat();

	shipLaunchStage = data.get("ship_launch_stage", 0).asInt();

	{
		auto dockStages = data.get("dock_anim_stage_duration", Json::arrayValue);
		if (dockStages.size() < 1) {
			Output("couldn't parse station def '%s': dock_anim_stage_duration requires at least one stage\n", path.c_str());
			return;
		}
		numDockingStages = dockStages.size();
		for (auto i = dockStages.begin(); i != dockStages.end(); ++i)
			dockAnimStageDuration.push_back((*i).asFloat());
	}

	{
		auto undockStages = data.get("undock_anim_stage_duration", Json::arrayValue);
		if (undockStages.size() < 1) {
			Output("couldn't parse station def '%s': undock_anim_stage_duration requires at least one stage\n", path.c_str());
			return;
		}
		numUndockStages = undockStages.size();
		for (auto i = undockStages.begin(); i != undockStages.end(); ++i)
			undockAnimStageDuration.push_back((*i).asFloat());
	}

	{
		auto bayGroupData = data.get("bay_groups", Json::arrayValue);
		if (bayGroupData.size() < 1) {
			Output("couldn't parse station def '%s': bay_groups requires at least one group\n", path.c_str());
			return;
		}

		numDockingPorts = 0;
		for (auto i = bayGroupData.begin(); i != bayGroupData.end(); ++i) {
			auto bayData = (*i);
			SpaceStationType::SBayGroup newBay;
			newBay.minShipSize = bayData[0].asInt();
			newBay.maxShipSize = bayData[1].asInt();
			auto groupData = bayData[2];
			if (groupData.size() < 1) {
				Output("couldn't parse station def '%s': bay groups must have at least one bay\n", path.c_str());
				return;
			}
			for (auto j = groupData.begin(); j != groupData.end(); ++j) {
				newBay.bayIDs.push_back((*j).asUInt());
				numDockingPorts++;
			}
			bayGroups.push_back(newBay);
		}
	}

	assert(!modelName.empty());
	model = Pi::FindModel(modelName);

	SceneGraph::Model::TVecMT approach_mts;
	SceneGraph::Model::TVecMT docking_mts;
	SceneGraph::Model::TVecMT leaving_mts;
	model->FindTagsByStartOfName("approach_", approach_mts);
	model->FindTagsByStartOfName("docking_", docking_mts);
	model->FindTagsByStartOfName("leaving_", leaving_mts);

	{
		SceneGraph::Model::TVecMT::const_iterator apprIter = approach_mts.begin();
		for (; apprIter!=approach_mts.end() ; ++apprIter)
		{
			int bay, stage;
			PiVerify(2 == sscanf((*apprIter)->GetName().c_str(), "approach_stage%d_bay%d", &stage, &bay));
			PiVerify(bay>0 && stage>0);
			SBayGroup* pGroup = GetGroupByBay(bay);
			assert(pGroup);
			pGroup->m_approach[stage] = (*apprIter)->GetTransform();
		}

		SceneGraph::Model::TVecMT::const_iterator dockIter = docking_mts.begin();
		for (; dockIter!=docking_mts.end() ; ++dockIter)
		{
			int bay, stage;
			PiVerify(2 == sscanf((*dockIter)->GetName().c_str(), "docking_stage%d_bay%d", &stage, &bay));
			PiVerify(bay>0 && stage>0);
			m_ports[bay].m_docking[stage+1] = (*dockIter)->GetTransform();
		}

		SceneGraph::Model::TVecMT::const_iterator leaveIter = leaving_mts.begin();
		for (; leaveIter!=leaving_mts.end() ; ++leaveIter)
		{
			int bay, stage;
			PiVerify(2 == sscanf((*leaveIter)->GetName().c_str(), "leaving_stage%d_bay%d", &stage, &bay));
			PiVerify(bay>0 && stage>0);
			m_ports[bay].m_leaving[stage] = (*leaveIter)->GetTransform();
		}

		assert(!m_ports.empty());
		assert(numDockingStages > 0);
		assert(numUndockStages > 0);

		for (PortMap::const_iterator pIt = m_ports.begin(), pItEnd = m_ports.end(); pIt!=pItEnd; ++pIt)
		{
			if (Uint32(numDockingStages-1) < pIt->second.m_docking.size()) {
				Error(
					"(%s): numDockingStages (%d) vs number of docking stages (" SIZET_FMT ")\n"
					"Must have at least the same number of entries as the number of docking stages "
					"PLUS the docking timeout at the start of the array.",
					modelName.c_str(), (numDockingStages-1), pIt->second.m_docking.size());

			} else if (Uint32(numDockingStages-1) != pIt->second.m_docking.size()) {
				Warning(
					"(%s): numDockingStages (%d) vs number of docking stages (" SIZET_FMT ")\n",
					modelName.c_str(), (numDockingStages-1), pIt->second.m_docking.size());
			}

			if (0!=pIt->second.m_leaving.size() && Uint32(numUndockStages) < pIt->second.m_leaving.size()) {
				Error(
					"(%s): numUndockStages (%d) vs number of leaving stages (" SIZET_FMT ")\n"
					"Must have at least the same number of entries as the number of leaving stages.",
					modelName.c_str(), (numDockingStages-1), pIt->second.m_docking.size());

			} else if(0!=pIt->second.m_leaving.size() && Uint32(numUndockStages) != pIt->second.m_leaving.size()) {
				Warning(
					"(%s): numUndockStages (%d) vs number of leaving stages (" SIZET_FMT ")\n",
					modelName.c_str(), numUndockStages, pIt->second.m_leaving.size());
			}

		}
	}
}

std::vector<SpaceStationType> SpaceStationType::surfaceTypes;
std::vector<SpaceStationType> SpaceStationType::orbitalTypes;

void SpaceStationType::Init()
{
	static bool isInitted = false;
	if (isInitted) return;
	isInitted = true;

	// load all station definitions
	namespace fs = FileSystem;
	for (fs::FileEnumerator files(fs::gameDataFiles, "stations", 0); !files.Finished(); files.Next()) {
		const fs::FileInfo &info = files.Current();
		if (ends_with_ci(info.GetPath(), ".json")) {
			const std::string id(info.GetName().substr(0, info.GetName().size()-4));
			SpaceStationType st = SpaceStationType(id, info.GetPath());
			switch (st.dockMethod) {
				case SURFACE: surfaceTypes.push_back(st); break;
				case ORBITAL: orbitalTypes.push_back(st); break;
			}
		}
	}
}
