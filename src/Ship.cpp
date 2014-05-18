// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Ship.h"
#include "CityOnPlanet.h"
#include "Lang.h"
#include "EnumStrings.h"
#include "Player.h"
#include "ShipAICmd.h"
#include "ShipController.h"
#include "Slice.h"
#include "Sfx.h"
#include "galaxy/Sector.h"
#include "galaxy/GalaxyCache.h"
#include "Frame.h"
#include "WorldView.h"
#include "HyperspaceCloud.h"
#include "graphics/Drawables.h"
#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/TextureBuilder.h"
#include "collider/collider.h"
#include "StringF.h"

static const double KINETIC_ENERGY_MULT	= 0.01;
HeatGradientParameters_t Ship::s_heatGradientParams;

void Ship::Save(Serializer::Writer &wr, Space *space)
{
	DynamicBody::Save(wr, space);
	m_skin.Save(wr);
	wr.Vector3d(m_angThrusters);
	wr.Vector3d(m_thrusters);
	wr.Int32(m_wheelTransition);
	wr.Float(m_wheelState);
	wr.Float(m_launchLockTimeout);
	wr.Int32(Uint32(m_sliceDriveState));
	wr.Float(m_sliceDriveStartTimeout);
	wr.Bool(m_testLanded);
	wr.Int32(Uint32(m_flightState));

	// XXX make sure all hyperspace attrs and the cloud get saved
	m_hyperspace.dest.Serialize(wr);
	wr.Float(m_hyperspace.countdown);

	wr.String(m_type->id);
	wr.Int32(m_dockedWithPort);
	wr.Int32(space->GetIndexForBody(m_dockedWith));
	wr.Float(m_stats.hull_mass_left);
	if(m_curAICmd) { wr.Int32(1); m_curAICmd->Save(wr); }
	else wr.Int32(0);
	wr.Int32(int(m_aiMessage));

	wr.Int32(static_cast<int>(m_controller->GetType()));
	m_controller->Save(wr, space);

	m_navLights->Save(wr);
}

void Ship::Load(Serializer::Reader &rd, Space *space)
{
	DynamicBody::Load(rd, space);
	m_skin.Load(rd);
	m_skin.Apply(GetModel());
	// needs fixups
	m_angThrusters = rd.Vector3d();
	m_thrusters = rd.Vector3d();
	m_wheelTransition = rd.Int32();
	m_wheelState = rd.Float();
	m_launchLockTimeout = rd.Float();
	m_sliceDriveState = static_cast<Slice::DriveState>(rd.Int32());
	m_sliceDriveStartTimeout = rd.Float();
	m_testLanded = rd.Bool();
	m_flightState = static_cast<FlightState>(rd.Int32());
	Properties().Set("flightState", EnumStrings::GetString("ShipFlightState", m_flightState));

	m_hyperspace.dest = SystemPath::Unserialize(rd);
	m_hyperspace.countdown = rd.Float();
	m_hyperspace.duration = 0;

	SetShipId(rd.String()); // XXX handle missing thirdparty ship
	m_dockedWithPort = rd.Int32();
	m_dockedWithIndex = rd.Int32();
	Init();
	m_stats.hull_mass_left = rd.Float(); // must be after Init()...
	if(rd.Int32()) m_curAICmd = AICommand::Load(rd);
	else m_curAICmd = 0;
	m_aiMessage = AIError(rd.Int32());

	PropertyMap &p = Properties();
	p.Set("hullMassLeft", m_stats.hull_mass_left);
	p.Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));

	m_controller = 0;
	const ShipController::Type ctype = static_cast<ShipController::Type>(rd.Int32());
	if (ctype == ShipController::PLAYER)
		SetController(new PlayerShipController());
	else
		SetController(new ShipController());
	m_controller->Load(rd);

	m_navLights->Load(rd);
}

void Ship::InitMaterials()
{
	SceneGraph::Model *pModel = GetModel();
	assert(pModel);
	const Uint32 numMats = pModel->GetNumMaterials();
	for( Uint32 m=0; m<numMats; m++ ) {
		RefCountedPtr<Graphics::Material> mat = pModel->GetMaterialByIndex(m);
		mat->heatGradient = Graphics::TextureBuilder::Decal("textures/heat_gradient.png").GetOrCreateTexture(Pi::renderer, "model");
		mat->specialParameter0 = &s_heatGradientParams;
	}
	s_heatGradientParams.heatingAmount = 0.0f;
	s_heatGradientParams.heatingNormal = vector3f(0.0f, -1.0f, 0.0f);
}

void Ship::Init()
{
	m_invulnerable = false;

	m_sensors.reset(new Sensors(this));

	m_navLights.reset(new NavLights(GetModel()));
	m_navLights->SetEnabled(true);

	SetMass(m_type->hullMass*1000.0f);
	SetMassDistributionFromModel();
	m_stats.hull_mass_left = float(m_type->hullMass);

	PropertyMap &p = Properties();
	p.Set("hullMassLeft", m_stats.hull_mass_left);
	p.Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));

	m_hyperspace.now = false;			// TODO: move this on next savegame change, maybe
	m_hyperspaceCloud = 0;

	m_landingGearAnimation = GetModel()->FindAnimation("gear_down");

	// If we've got the tag_landing set then use it for an offset otherwise grab the AABB
	const SceneGraph::MatrixTransform *mt = GetModel()->FindTagByName("tag_landing");
	if( mt ) {
		m_landingMinOffset = mt->GetTransform().GetTranslate().y;
	} else {
		m_landingMinOffset = GetAabb().min.y;
	}

	InitMaterials();
}

void Ship::PostLoadFixup(Space *space)
{
	DynamicBody::PostLoadFixup(space);
	m_dockedWith = static_cast<SpaceStation*>(space->GetBodyByIndex(m_dockedWithIndex));
	if (m_curAICmd) m_curAICmd->PostLoadFixup(space);
	m_controller->PostLoadFixup(space);
}

Ship::Ship(const std::string &shipId): DynamicBody(),
	m_controller(0),
	m_landingGearAnimation(nullptr)
{
	m_flightState = FLYING;
	Properties().Set("flightState", EnumStrings::GetString("ShipFlightState", m_flightState));
	m_sliceDriveState = Slice::DriveState::DRIVE_OFF;

	m_testLanded = false;
	m_launchLockTimeout = 0;
	m_sliceDriveStartTimeout = 0.0f;
	m_wheelTransition = 0;
	m_wheelState = 0;
	m_dockedWith = 0;
	m_dockedWithPort = 0;
	SetShipId(shipId);
	m_thrusters.x = m_thrusters.y = m_thrusters.z = 0;
	m_angThrusters.x = m_angThrusters.y = m_angThrusters.z = 0;
	m_hyperspace.countdown = 0;
	m_hyperspace.now = false;
	m_curAICmd = 0;
	m_aiMessage = AIERROR_NONE;
	m_decelerating = false;

	SetModel(m_type->model.c_str());
	SetLabel("UNLABELED_SHIP");
	m_skin.SetRandomColors(Pi::rng);
	m_skin.Apply(GetModel());
	GetModel()->SetPattern(Pi::rng.Int32(0, GetModel()->GetNumPatterns()));

	Init();
	SetController(new ShipController());
}

Ship::~Ship()
{
	if (m_curAICmd) delete m_curAICmd;
	delete m_controller;
}

void Ship::SetController(ShipController *c)
{
	assert(c != 0);
	if (m_controller) delete m_controller;
	m_controller = c;
	m_controller->m_ship = this;
}

float Ship::GetPercentHull() const
{
	return 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass));
}

void Ship::SetPercentHull(float p)
{
	m_stats.hull_mass_left = 0.01f * Clamp(p, 0.0f, 100.0f) * float(m_type->hullMass);
	Properties().Set("hullMassLeft", m_stats.hull_mass_left);
	Properties().Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
}

bool Ship::OnDamage(Object *attacker, float kgDamage, const CollisionContact& contactData)
{
	if (m_invulnerable)
		return true;

	if (!IsDead()) {
		float dam = kgDamage*0.001f;

		m_stats.hull_mass_left -= dam;
		Properties().Set("hullMassLeft", m_stats.hull_mass_left);
		Properties().Set("hullPercent", 100.0f * (m_stats.hull_mass_left / float(m_type->hullMass)));
		if (m_stats.hull_mass_left < 0) {
			Explode();
		} else {
			if (Pi::rng.Double() < kgDamage)
				Sfx::Add(this, Sfx::TYPE_DAMAGE);
		}
	}

	//Output("Ouch! %s took %.1f kilos of damage from %s! (%.1f t hull left)\n", GetLabel().c_str(), kgDamage, attacker->GetLabel().c_str(), m_stats.hull_mass_left);
	return true;
}

bool Ship::OnCollision(Object *b, Uint32 flags, double relVel)
{
	// hitting space station docking surfaces shouldn't do damage
	if (b->IsType(Object::SPACESTATION) && (flags & 0x10)) {
		return true;
	}

	if (b->IsType(Object::PLANET)) {
		// geoms still enabled when landed
		if (m_flightState != FLYING) return false;
		else {
			if (GetVelocity().Length() < MAX_LANDING_SPEED) {
				m_testLanded = true;
				return true;
			}
		}
	}

	return DynamicBody::OnCollision(b, flags, relVel);
}

//destroy ship in an explosion
void Ship::Explode()
{
	if (m_invulnerable) return;

	Pi::game->GetSpace()->KillBody(this);
	if (this->GetFrame() == Pi::player->GetFrame()) {
		Sfx::AddExplosion(this, Sfx::TYPE_EXPLOSION);
	}
	ClearThrusterState();
}

void Ship::SetThrusterState(const vector3d &levels)
{
	m_thrusters.x = Clamp(levels.x, -1.0, 1.0);
	m_thrusters.y = Clamp(levels.y, -1.0, 1.0);
	m_thrusters.z = Clamp(levels.z, -1.0, 1.0);
}

void Ship::SetAngThrusterState(const vector3d &levels)
{
	m_angThrusters.x = Clamp(levels.x, -1.0, 1.0);
	m_angThrusters.y = Clamp(levels.y, -1.0, 1.0);
	m_angThrusters.z = Clamp(levels.z, -1.0, 1.0);
}

vector3d Ship::GetMaxThrust(const vector3d &dir) const
{
	vector3d maxThrust;
	maxThrust.x = (dir.x > 0) ? m_type->linThrust[ShipType::THRUSTER_RIGHT]
		: -m_type->linThrust[ShipType::THRUSTER_LEFT];
	maxThrust.y = (dir.y > 0) ? m_type->linThrust[ShipType::THRUSTER_UP]
		: -m_type->linThrust[ShipType::THRUSTER_DOWN];
	maxThrust.z = (dir.z > 0) ? m_type->linThrust[ShipType::THRUSTER_REVERSE]
		: -m_type->linThrust[ShipType::THRUSTER_FORWARD];
	return maxThrust;
}

double Ship::GetAccelMin() const
{
	float val = m_type->linThrust[ShipType::THRUSTER_UP];
	val = std::min(val, m_type->linThrust[ShipType::THRUSTER_RIGHT]);
	val = std::min(val, -m_type->linThrust[ShipType::THRUSTER_LEFT]);
	return val / GetMass();
}

void Ship::ClearThrusterState()
{
	m_angThrusters = vector3d(0,0,0);
	if (m_launchLockTimeout <= 0.0f) m_thrusters = vector3d(0,0,0);
}

Ship::HyperjumpStatus Ship::GetHyperspaceDetails(const SystemPath &src, const SystemPath &dest, double &outDurationSecs)
{
	assert(dest.HasValidSystem());

	outDurationSecs = 5.0f;

	if (src.IsSameSystem(dest))
		return HYPERJUMP_CURRENT_SYSTEM;

	return GetFlightState() == JUMPING ? HYPERJUMP_INITIATED : HYPERJUMP_OK;
}

Ship::HyperjumpStatus Ship::GetHyperspaceDetails(const SystemPath &dest, double &outDurationSecs)
{
	if (GetFlightState() == HYPERSPACE) {
		outDurationSecs = 0.0;
		return HYPERJUMP_DRIVE_ACTIVE;
	}
	return GetHyperspaceDetails(Pi::game->GetSpace()->GetStarSystem()->GetSystemPath(), dest, outDurationSecs);
}

Ship::HyperjumpStatus Ship::CheckHyperjumpCapability() const {
	if (GetFlightState() == HYPERSPACE)
		return HYPERJUMP_DRIVE_ACTIVE;

	if (GetFlightState() != FLYING && GetFlightState() != JUMPING)
		return HYPERJUMP_SAFETY_LOCKOUT;

	return HYPERJUMP_OK;
}

Ship::HyperjumpStatus Ship::CheckHyperspaceTo(const SystemPath &dest, double &outDurationSecs)
{
	assert(dest.HasValidSystem());

	outDurationSecs = 0.0;

	if (GetFlightState() != FLYING && GetFlightState() != JUMPING)
		return HYPERJUMP_SAFETY_LOCKOUT;

	return GetHyperspaceDetails(dest, outDurationSecs);
}

Ship::HyperjumpStatus Ship::InitiateHyperjumpTo(const SystemPath &dest, int warmup_time, double duration) {
	if (!dest.HasValidSystem() || GetFlightState() != FLYING || warmup_time < 1)
		return HYPERJUMP_SAFETY_LOCKOUT;
	StarSystem *s = Pi::game->GetSpace()->GetStarSystem().Get();
	if (s && s->GetSystemPath().IsSameSystem(dest))
		return HYPERJUMP_CURRENT_SYSTEM;

	m_hyperspace.dest = dest;
	m_hyperspace.countdown = warmup_time;
	m_hyperspace.now = false;
	m_hyperspace.duration = duration;

	return Ship::HYPERJUMP_OK;
}

void Ship::AbortHyperjump() {
	m_hyperspace.countdown = 0;
	m_hyperspace.now = false;
	m_hyperspace.duration = 0;
}

Ship::HyperjumpStatus Ship::StartHyperspaceCountdown(const SystemPath &dest)
{
	HyperjumpStatus status = CheckHyperspaceTo(dest);
	if (status != HYPERJUMP_OK)
		return status;

	m_hyperspace.dest = dest;

	m_hyperspace.countdown = 3.0f;
	m_hyperspace.now = false;

	return Ship::HYPERJUMP_OK;
}

void Ship::ResetHyperspaceCountdown()
{
	m_hyperspace.countdown = 0;
	m_hyperspace.now = false;
}

void Ship::SetFlightState(const FlightState newState)
{
	if (m_flightState == newState) return;
	if (IsHyperspaceActive() && (newState != FLYING))
		ResetHyperspaceCountdown();

	if (newState == FLYING) {
		m_testLanded = false;
		if (m_flightState == DOCKING || m_flightState == DOCKED) onUndock.emit();
		m_dockedWith = 0;
		// lock thrusters for two seconds to push us out of station
		m_launchLockTimeout = 2.0;
	}

	m_flightState = newState;
	Properties().Set("flightState", EnumStrings::GetString("ShipFlightState", m_flightState));

	switch (m_flightState)
	{
		case FLYING:		SetMoving(true);	SetColliding(true);		SetStatic(false);	break;
		case DOCKING:		SetMoving(false);	SetColliding(false);	SetStatic(false);	break;
// TODO: set collision index? dynamic stations... use landed for open-air?
		case DOCKED:		SetMoving(false);	SetColliding(false);	SetStatic(false);	break;
		case LANDED:		SetMoving(false);	SetColliding(true);		SetStatic(true);	break;
		case JUMPING:		SetMoving(true);	SetColliding(false);	SetStatic(false);	break;
		case HYPERSPACE:	SetMoving(false);	SetColliding(false);	SetStatic(false);	break;
	}
}

void Ship::Blastoff()
{
	if (m_flightState != LANDED) return;

	vector3d up = GetPosition().Normalized();
	assert(GetFrame()->GetBody()->IsType(Object::PLANET));
	const double planetRadius = 2.0 + static_cast<Planet*>(GetFrame()->GetBody())->GetTerrainHeight(up);
	SetVelocity(vector3d(0, 0, 0));
	SetAngVelocity(vector3d(0, 0, 0));
	SetFlightState(FLYING);

	SetPosition(up*planetRadius - GetAabb().min.y*up);
	SetThrusterState(1, 1.0);		// thrust upwards
}

void Ship::TestLanded()
{
	m_testLanded = false;
	if (m_launchLockTimeout > 0.0f) return;
	if (m_wheelState < 1.0f) return;
	if (GetFrame()->GetBody()->IsType(Object::PLANET)) {
		double speed = GetVelocity().Length();
		vector3d up = GetPosition().Normalized();
		const double planetRadius = static_cast<Planet*>(GetFrame()->GetBody())->GetTerrainHeight(up);

		if (speed < MAX_LANDING_SPEED) {
			// check player is sortof sensibly oriented for landing
			if (GetOrient().VectorY().Dot(up) > 0.99) {
				// position at zero altitude
				SetPosition(up * (planetRadius - GetAabb().min.y));

				// position facing in roughly the same direction
				vector3d right = up.Cross(GetOrient().VectorZ()).Normalized();
				SetOrient(matrix3x3d::FromVectors(right, up));

				SetVelocity(vector3d(0, 0, 0));
				SetAngVelocity(vector3d(0, 0, 0));
				ClearThrusterState();
				SetFlightState(LANDED);
			}
		}
	}
}

void Ship::SetLandedOn(Planet *p, float latitude, float longitude)
{
	m_wheelTransition = 0;
	m_wheelState = 1.0f;
	Frame* f = p->GetFrame()->GetRotFrame();
	SetFrame(f);
	vector3d up = vector3d(cos(latitude)*sin(longitude), sin(latitude), cos(latitude)*cos(longitude));
	const double planetRadius = p->GetTerrainHeight(up);
	SetPosition(up * (planetRadius - GetAabb().min.y));
	vector3d right = up.Cross(vector3d(0,0,1)).Normalized();
	SetOrient(matrix3x3d::FromVectors(right, up));
	SetVelocity(vector3d(0, 0, 0));
	SetAngVelocity(vector3d(0, 0, 0));
	ClearThrusterState();
	SetFlightState(LANDED);
}

void Ship::SetFrame(Frame *f)
{
	DynamicBody::SetFrame(f);
	m_sensors->ResetTrails();
}

void Ship::TimeStepUpdate(const float timeStep)
{
	// If docked, station is responsible for updating position/orient of ship
	// but we call this crap anyway and hope it doesn't do anything bad

	const vector3d maxThrust = GetMaxThrust(m_thrusters);
	const vector3d thrust = vector3d(maxThrust.x*m_thrusters.x, maxThrust.y*m_thrusters.y, maxThrust.z*m_thrusters.z);
	AddRelForce(thrust);
	AddRelTorque(GetShipType()->angThrust * m_angThrusters);

	if (m_landingGearAnimation)
		m_landingGearAnimation->SetProgress(m_wheelState);

	DynamicBody::TimeStepUpdate(timeStep);

	m_navLights->SetEnabled(m_wheelState > 0.01f);
	m_navLights->Update(timeStep);
	if (m_sensors.get()) m_sensors->Update(timeStep);
}

// for timestep changes, to stop autopilot overshoot
// either adds half of current accel if decelerating
void Ship::TimeAccelAdjust(const float timeStep)
{
	if (!AIIsActive()) return;
#ifdef DEBUG_AUTOPILOT
	if (this->IsType(Object::PLAYER))
		Output("Time accel adjustment, step = %.1f, decel = %s\n", double(timeStep),
			m_decelerating ? "true" : "false");
#endif
	vector3d vdiff = double(timeStep) * GetLastForce() * (1.0 / GetMass());
	if (!m_decelerating) vdiff = -2.0 * vdiff;
	SetVelocity(GetVelocity() + vdiff);
}

double Ship::GetHullTemperature() const
{
	double dragGs = GetAtmosForce().Length() / (GetMass() * 9.81);
	return dragGs / 300.0;
}

void Ship::StaticUpdate(const float timeStep)
{
	if (IsDead()) return;

	if (m_controller) m_controller->StaticUpdate(timeStep);

	if (GetHullTemperature() > 1.0)
		Explode();

	if (m_flightState == FLYING)
		m_launchLockTimeout -= timeStep;
	if (m_launchLockTimeout < 0) m_launchLockTimeout = 0;
	if (m_flightState == JUMPING || m_flightState == HYPERSPACE)
		m_launchLockTimeout = 0;

	if (m_wheelTransition) {
		m_wheelState += m_wheelTransition*0.3f*timeStep;
		m_wheelState = Clamp(m_wheelState, 0.0f, 1.0f);
		if (is_equal_exact(m_wheelState, 0.0f) || is_equal_exact(m_wheelState, 1.0f))
			m_wheelTransition = 0;
	}

	if (m_testLanded) TestLanded();

	// After calling StartHyperspaceTo this Ship must not spawn objects
	// holding references to it (eg missiles), as StartHyperspaceTo
	// removes the ship from Space::bodies and so the missile will not
	// have references to this cleared by NotifyRemoved()
	if (m_hyperspace.now) {
		m_hyperspace.now = false;
		EnterHyperspace();
	}

	if (m_hyperspace.countdown > 0.0f) {
		m_hyperspace.countdown = m_hyperspace.countdown - timeStep;
		if (m_hyperspace.countdown <= 0.0f) {
			m_hyperspace.countdown = 0;
			m_hyperspace.now = true;
			SetFlightState(JUMPING);
		}
	}

	//play start transit drive
	if (GetSliceDriveState() == Slice::DriveState::DRIVE_READY && IsType(Object::PLAYER)) {
		SetSliceDriveState(Slice::DriveState::DRIVE_START);
		m_sliceDriveStartTimeout = 2.0f;	// XXX - hack, need to read from a config or wait for some timeout event like audio of drive warming up etc.
	}
	if(GetSliceDriveState() == Slice::DriveState::DRIVE_START && IsType(Object::PLAYER)) {
		if(m_sliceDriveStartTimeout > 0.0f) {
			m_sliceDriveStartTimeout -= timeStep;
		} else {
			m_sliceDriveStartTimeout = 0.0f;
			SetSliceDriveState(Slice::DriveState::DRIVE_ON);
		}
	}
	//play stop transit drive
	if (GetSliceDriveState() == Slice::DriveState::DRIVE_STOP && IsType(Object::PLAYER) ) {
		SetSliceDriveState(Slice::DriveState::DRIVE_FINISHED);
	}
}

void Ship::EngageSliceDrive()
{
	if(GetSliceDriveState() == Slice::DriveState::DRIVE_OFF && IsType(Object::PLAYER)) {
		SetSliceDriveState(Slice::DriveState::DRIVE_READY);
	}
}

void Ship::DisengageSliceDrive()
{
	if(GetSliceDriveState() != Slice::DriveState::DRIVE_OFF && IsType(Object::PLAYER)) {
		// Transit interrupted
		//float interrupt_velocity = GetMaxManeuverSpeed();
		float interrupt_velocity = 1000.0;
		SetVelocity(GetOrient()*vector3d(0, 0, -interrupt_velocity));
		SetSliceDriveState(Slice::DriveState::DRIVE_OFF);
	}
}

void Ship::NotifyRemoved(const Body* const removedBody)
{
	if (m_curAICmd) m_curAICmd->OnDeleted(removedBody);
}

bool Ship::Undock()
{
	return (m_dockedWith && m_dockedWith->LaunchShip(this, m_dockedWithPort));
}

void Ship::SetDockedWith(SpaceStation *s, int port)
{
	if (s) {
		m_dockedWith = s;
		m_dockedWithPort = port;
		m_wheelTransition = 0;
		m_wheelState = 1.0f;
		// hand position/state responsibility over to station
		m_dockedWith->SetDocked(this, port);
		onDock.emit();
	} else {
		Undock();
	}
}

bool Ship::SetWheelState(bool down)
{
	if (m_flightState != FLYING) return false;
	if (is_equal_exact(m_wheelState, down ? 1.0f : 0.0f)) return false;
	int newWheelTransition = (down ? 1 : -1);
	if (newWheelTransition == m_wheelTransition) return false;
	m_wheelTransition = newWheelTransition;
	return true;
}

void Ship::Render(Graphics::Renderer *renderer, const Camera *camera, const vector3d &viewCoords, const matrix4x4d &viewTransform)
{
	if (IsDead()) return;

	//angthrust negated, for some reason
	GetModel()->SetThrust(vector3f(m_thrusters), -vector3f(m_angThrusters));

	matrix3x3f mt;
	matrix3x3dtof(viewTransform.Inverse().GetOrient(), mt);
	s_heatGradientParams.heatingMatrix = mt;
	s_heatGradientParams.heatingNormal = vector3f(GetVelocity().Normalized());
	s_heatGradientParams.heatingAmount = Clamp(GetHullTemperature(),0.0,1.0);

	//strncpy(params.pText[0], GetLabel().c_str(), sizeof(params.pText));
	RenderModel(renderer, camera, viewCoords, viewTransform);
}

void Ship::EnterHyperspace() {
	assert(GetFlightState() != Ship::HYPERSPACE);

	Ship::HyperjumpStatus status = CheckHyperjumpCapability();
	if (status != HYPERJUMP_OK && status != HYPERJUMP_INITIATED) {
		if (m_flightState == JUMPING)
			SetFlightState(FLYING);
		return;
	}

	m_hyperspace.duration = 5.0f;

	SetFlightState(Ship::HYPERSPACE);

	// virtual call, do class-specific things
	OnEnterHyperspace();
}

void Ship::OnEnterHyperspace() {
	m_hyperspaceCloud = new HyperspaceCloud(this, Pi::game->GetTime() + m_hyperspace.duration, false);
	m_hyperspaceCloud->SetFrame(GetFrame());
	m_hyperspaceCloud->SetPosition(GetPosition());

	Space *space = Pi::game->GetSpace();

	space->RemoveBody(this);
	space->AddBody(m_hyperspaceCloud);
}

void Ship::EnterSystem() {
	PROFILE_SCOPED()
	assert(GetFlightState() == Ship::HYPERSPACE);

	// virtual call, do class-specific things
	OnEnterSystem();

	SetFlightState(Ship::FLYING);
}

void Ship::OnEnterSystem() {
	m_hyperspaceCloud = 0;
}

void Ship::SetShipId(const std::string &shipId)
{
	auto i = ShipType::types.find(shipId);
	assert(i != ShipType::types.end());
	m_type = &((*i).second);
	Properties().Set("shipId", shipId);
}

void Ship::SetShipType(const std::string &shipId)
{
	SetShipId(shipId);
	SetModel(m_type->model.c_str());
	m_skin.Apply(GetModel());
	Init();
	onFlavourChanged.emit();
	if (IsType(Object::PLAYER))
		Pi::worldView->SetCamType(Pi::worldView->GetCamType());
}

void Ship::SetLabel(const std::string &label)
{
	DynamicBody::SetLabel(label);
	m_skin.SetLabel(label);
	m_skin.Apply(GetModel());
}

void Ship::SetSkin(const SceneGraph::ModelSkin &skin)
{
	m_skin = skin;
	m_skin.Apply(GetModel());
}

Uint8 Ship::GetRelations(Body *other) const
{
	auto it = m_relationsMap.find(other);
	if (it != m_relationsMap.end())
		return it->second;

	return 50;
}

void Ship::SetRelations(Body *other, Uint8 percent)
{
	m_relationsMap[other] = percent;
	if (m_sensors.get()) m_sensors->UpdateIFF(other);
}
