// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SHIP_H
#define _SHIP_H

#include "libs.h"
#include "Camera.h"
#include "DynamicBody.h"
#include "galaxy/SystemPath.h"
#include "HudTrail.h"
#include "NavLights.h"
#include "Planet.h"
#include "Sensors.h"
#include "Serializer.h"
#include "ShipType.h"
#include "scenegraph/SceneGraph.h"
#include "scenegraph/ModelSkin.h"
#include <list>
#include <unordered_map>

class SpaceStation;
class HyperspaceCloud;
class AICommand;
class ShipController;
namespace Graphics { class Renderer; }

struct HeatGradientParameters_t {
	matrix3x3f heatingMatrix;
	vector3f heatingNormal; // normalised
	float heatingAmount; // 0.0 to 1.0 used for `u` component of heatGradient texture
};

struct shipstats_t {
	int total_mass; // hull
	float hull_mass_left; // effectively hitpoints
	float hyperspace_range;
	float hyperspace_range_max;
};

class Ship: public DynamicBody {
	friend class ShipController; //only controllers need access to AITimeStep
	friend class PlayerShipController;
public:
	OBJDEF(Ship, DynamicBody, SHIP);
	Ship(const std::string &shipId);
	Ship() {} //default constructor used before Load
	virtual ~Ship();

	virtual void SetFrame(Frame *f);

	void SetController(ShipController *c); //deletes existing
	ShipController *GetController() const { return m_controller; }
	virtual bool IsPlayerShip() const { return false; } //XXX to be replaced with an owner check

	virtual void SetDockedWith(SpaceStation *, int port);
	/** Use GetDockedWith() to determine if docked */
	SpaceStation *GetDockedWith() const { return m_dockedWith; }
	int GetDockingPort() const { return m_dockedWithPort; }

	virtual void SetLandedOn(Planet *p, float latitude, float longitude);

	virtual void Render(Graphics::Renderer *r, const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform);

	void SetThrusterState(int axis, double level) {
		m_thrusters[axis] = Clamp(level, -1.0, 1.0);
	}
	void SetThrusterState(const vector3d &levels);
	vector3d GetThrusterState() const { return m_thrusters; }
	void SetAngThrusterState(int axis, double level) { m_angThrusters[axis] = Clamp(level, -1.0, 1.0); }
	void SetAngThrusterState(const vector3d &levels);
	vector3d GetAngThrusterState() const { return m_angThrusters; }
	void ClearThrusterState();

	vector3d GetMaxThrust(const vector3d &dir) const;
	double GetAccelFwd() const { return -m_type->linThrust[ShipType::THRUSTER_FORWARD] / GetMass(); }
	double GetAccelRev() const { return m_type->linThrust[ShipType::THRUSTER_REVERSE] / GetMass(); }
	double GetAccelUp() const { return m_type->linThrust[ShipType::THRUSTER_UP] / GetMass(); }
	double GetAccelMin() const;

	const shipstats_t &GetStats() const { return m_stats; }

	void Explode();
	void UpdateMass();
	virtual bool SetWheelState(bool down); // returns success of state change, NOT state itself
	void Blastoff();
	bool Undock();
	virtual void TimeStepUpdate(const float timeStep);
	virtual void StaticUpdate(const float timeStep);

	void TimeAccelAdjust(const float timeStep);
	void SetDecelerating(bool decel) { m_decelerating = decel; }
	bool IsDecelerating() const { return m_decelerating; }

	virtual void NotifyRemoved(const Body* const removedBody);
	virtual bool OnCollision(Object *o, Uint32 flags, double relVel);
	virtual bool OnDamage(Object *attacker, float kgDamage, const CollisionContact& contactData);

	enum FlightState { // <enum scope='Ship' name=ShipFlightState public>
		FLYING,     // open flight (includes autopilot)
		DOCKING,    // in docking animation
		DOCKED,     // docked with station
		LANDED,     // rough landed (not docked)
		JUMPING,    // between space and hyperspace ;)
		HYPERSPACE, // in hyperspace
	};

	FlightState GetFlightState() const { return m_flightState; }
	void SetFlightState(FlightState s);
	float GetWheelState() const { return m_wheelState; }
	int GetWheelTransition() const { return m_wheelTransition; }

	virtual bool IsInSpace() const { return (m_flightState != HYPERSPACE); }

	void SetHyperspaceDest(const SystemPath &dest) { m_hyperspace.dest = dest; }
	const SystemPath &GetHyperspaceDest() const { return m_hyperspace.dest; }
	double GetHyperspaceDuration() const { return m_hyperspace.duration; }

	enum HyperjumpStatus { // <enum scope='Ship' name=ShipJumpStatus prefix=HYPERJUMP_ public>
		HYPERJUMP_OK,
		HYPERJUMP_CURRENT_SYSTEM,
		HYPERJUMP_NO_DRIVE,
		HYPERJUMP_INITIATED,
		HYPERJUMP_DRIVE_ACTIVE,
		HYPERJUMP_SAFETY_LOCKOUT
	};

	HyperjumpStatus GetHyperspaceDetails(const SystemPath &src, const SystemPath &dest, double &outDurationSecs);
	HyperjumpStatus GetHyperspaceDetails(const SystemPath &dest, double &outDurationSecs);
	HyperjumpStatus CheckHyperspaceTo(const SystemPath &dest, double &outDurationSecs);
	HyperjumpStatus CheckHyperspaceTo(const SystemPath &dest) {
		double unusedDuration;
		return CheckHyperspaceTo(dest, unusedDuration);
	}
	bool CanHyperspaceTo(const SystemPath &dest, HyperjumpStatus &status) {
		status = CheckHyperspaceTo(dest);
		return (status == HYPERJUMP_OK);
	}
	bool CanHyperspaceTo(const SystemPath &dest) { return (CheckHyperspaceTo(dest) == HYPERJUMP_OK); }

	Ship::HyperjumpStatus CheckHyperjumpCapability() const;
	virtual Ship::HyperjumpStatus InitiateHyperjumpTo(const SystemPath &dest, int warmup_time, double duration);
	virtual void AbortHyperjump();
	virtual Ship::HyperjumpStatus StartHyperspaceCountdown(const SystemPath &dest);
	float GetHyperspaceCountdown() const { return m_hyperspace.countdown; }
	bool IsHyperspaceActive() const { return (m_hyperspace.countdown > 0.0); }
	virtual void ResetHyperspaceCountdown();

	// 0 to 1.0 is alive, > 1.0 = death
	double GetHullTemperature() const;

	bool AIMatchVel(const vector3d &vel);
	bool AIChangeVelBy(const vector3d &diffvel);		// acts in obj space
	vector3d AIChangeVelDir(const vector3d &diffvel);	// world space, maintain direction
	void AIMatchAngVelObjSpace(const vector3d &angvel);
	double AIFaceUpdir(const vector3d &updir, double av=0);
	double AIFaceDirection(const vector3d &dir, double av=0);
	double AITravelTime(const vector3d &reldir, double targdist, const vector3d &relvel, double endspeed, double maxdecel);

	// old stuff, deprecated
	void AIAccelToModelRelativeVelocity(const vector3d &v);
	void AIModelCoordsMatchAngVel(const vector3d &desiredAngVel, double softness);
	void AIModelCoordsMatchSpeedRelTo(const vector3d &v, const Ship *);

	void AIClearInstructions();
	bool AIIsActive() { return m_curAICmd ? true : false; }
	void AIGetStatusText(char *str);

	enum AIError { // <enum scope='Ship' name=ShipAIError prefix=AIERROR_ public>
		AIERROR_NONE=0,
		AIERROR_GRAV_TOO_HIGH,
		AIERROR_REFUSED_PERM,
		AIERROR_ORBIT_IMPOSSIBLE
	};
	AIError AIMessage(AIError msg=AIERROR_NONE) {
		AIError tmp = m_aiMessage;
		m_aiMessage = msg;
		Output("AI message changed from %d to %d\n", tmp, m_aiMessage);
		return tmp;
	}

	void AIKamikaze(Body *target);
	//void AIJourney(SystemBodyPath &dest);
	void AIDock(SpaceStation *target);
	void AIFlyTo(Body *target);
	void AIOrbit(Body *target, double alt);
	void AIHoldPosition();

	void AIBodyDeleted(const Body* const body) {};		// todo: signals

	virtual void PostLoadFixup(Space *space);

	const ShipType *GetShipType() const { return m_type; }
	void SetShipType(const std::string &shipId);

	const SceneGraph::ModelSkin &GetSkin() const { return m_skin; }
	void SetSkin(const SceneGraph::ModelSkin &skin);

	void SetLabel(const std::string &label);

	float GetPercentHull() const;
	void SetPercentHull(float);

	void EnterSystem();

	HyperspaceCloud *GetHyperspaceCloud() const { return m_hyperspaceCloud; }

	sigc::signal<void> onDock;				// JJ: check what these are for
	sigc::signal<void> onUndock;

	// mutable because asking to know when state changes is not the same as
	// actually changing state
	mutable sigc::signal<void> onFlavourChanged;

	bool IsInvulnerable() const { return m_invulnerable; }
	void SetInvulnerable(bool b) { m_invulnerable = b; }

	Sensors *GetSensors() const { return m_sensors.get(); }

	Uint8 GetRelations(Body *other) const; //0=hostile, 50=neutral, 100=ally
	void SetRelations(Body *other, Uint8 percent);

	double GetLandingPosOffset() const { return m_landingMinOffset; }

protected:
	virtual void Save(Serializer::Writer &wr, Space *space);
	virtual void Load(Serializer::Reader &rd, Space *space);
	void RenderLaserfire();

	bool AITimeStep(float timeStep); // Called by controller. Returns true if complete

	virtual void OnEnterHyperspace();
	virtual void OnEnterSystem();

	SpaceStation *m_dockedWith;
	int m_dockedWithPort;

	ShipController *m_controller;

private:
	void FireWeapon(int num);
	void Init();
	bool IsFiringLasers();
	void TestLanded();
    void SetShipId(const std::string &shipId);
	void EnterHyperspace();
	void InitMaterials();

	bool m_invulnerable;

	shipstats_t m_stats;
	const ShipType *m_type;
	SceneGraph::ModelSkin m_skin;

	FlightState m_flightState;
	bool m_testLanded;
	float m_launchLockTimeout;
	float m_wheelState;
	int m_wheelTransition;

	vector3d m_thrusters;
	vector3d m_angThrusters;

	struct HyperspacingOut {
		SystemPath dest;
		// > 0 means active
		float countdown;
		bool now;
		double duration;
	} m_hyperspace;
	HyperspaceCloud *m_hyperspaceCloud;

	AICommand *m_curAICmd;
	AIError m_aiMessage;
	bool m_decelerating;

	double m_landingMinOffset;	// offset from the centre of the ship used during docking

	int m_dockedWithIndex; // deserialisation

	SceneGraph::Animation *m_landingGearAnimation;
	std::unique_ptr<NavLights> m_navLights;

	static HeatGradientParameters_t s_heatGradientParams;

	std::unique_ptr<Sensors> m_sensors;
	std::unordered_map<Body*, Uint8> m_relationsMap;
};



#endif /* _SHIP_H */


