// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ShipController.h"
#include "Frame.h"
#include "Game.h"
#include "KeyBindings.h"
#include "Pi.h"
#include "Player.h"
#include "Ship.h"
#include "Slice.h"
#include "Space.h"
#include "WorldView.h"
#include "OS.h"

void ShipController::StaticUpdate(float timeStep)
{
	OS::EnableFPE();
	m_ship->AITimeStep(timeStep);
	OS::DisableFPE();
}

PlayerShipController::PlayerShipController() :
	ShipController(),
	m_navTarget(0),
	m_setSpeedTarget(0),
	m_controlsLocked(false),
	m_invertMouse(false),
	m_mouseActive(false),
	m_rotationDamping(true),
	m_mouseX(0.0),
	m_mouseY(0.0),
	m_setSpeed(0.0),
	m_flightControlState(FlightControlState::CONTROL_MANUAL),
	m_lowThrustPower(0.25), // note: overridden by the default value in GameConfig.cpp (DefaultLowThrustPower setting)
	m_mouseDir(0.0)
{
	float deadzone = Pi::config->Float("JoystickDeadzone");
	m_joystickDeadzone = deadzone * deadzone;
	m_fovY = Pi::config->Float("FOVVertical");
	m_lowThrustPower = Pi::config->Float("DefaultLowThrustPower");

	m_connRotationDampingToggleKey = KeyBindings::toggleRotationDamping.onPress.connect(
			sigc::mem_fun(this, &PlayerShipController::ToggleRotationDamping));
}

PlayerShipController::~PlayerShipController()
{
	m_connRotationDampingToggleKey.disconnect();
	m_fireMissileKey.disconnect();
}

void PlayerShipController::Save(Serializer::Writer &wr, Space *space)
{
	wr.Int32(static_cast<int>(m_flightControlState));
	wr.Double(m_setSpeed);
	wr.Float(m_lowThrustPower);
	wr.Bool(m_rotationDamping);
	wr.Int32(space->GetIndexForBody(m_navTarget));
	wr.Int32(space->GetIndexForBody(m_setSpeedTarget));
}

void PlayerShipController::Load(Serializer::Reader &rd)
{
	m_flightControlState = static_cast<FlightControlState>(rd.Int32());
	m_setSpeed = rd.Double();
	m_lowThrustPower = rd.Float();
	m_rotationDamping = rd.Bool();
	//figure out actual bodies in PostLoadFixup - after Space body index has been built
	m_navTargetIndex = rd.Int32();
	m_setSpeedTargetIndex = rd.Int32();
}

void PlayerShipController::PostLoadFixup(Space *space)
{
	m_navTarget = space->GetBodyByIndex(m_navTargetIndex);
	m_setSpeedTarget = space->GetBodyByIndex(m_setSpeedTargetIndex);
}

void PlayerShipController::StaticUpdate(const float timeStep)
{
	vector3d v;
	matrix4x4d m;
	
	if (m_ship->GetFlightState() == Ship::FLYING) {
		switch (m_flightControlState) {
		case FlightControlState::CONTROL_FIXSPEED:
			PollControls(timeStep, true);
			if (IsAnyLinearThrusterKeyDown()) break;
			v = -m_ship->GetOrient().VectorZ() * m_setSpeed;
			if (m_setSpeedTarget) {
				v += m_setSpeedTarget->GetVelocityRelTo(m_ship->GetFrame());
			}
			m_ship->AIMatchVel(v);
			break;
		case FlightControlState::CONTROL_MANUAL:
			PollControls(timeStep, false);
			break;
		case FlightControlState::CONTROL_AUTOPILOT:
			if (m_ship->AIIsActive()) break;
			Pi::game->RequestTimeAccel(Game::TIMEACCEL_1X);
//			AIMatchVel(vector3d(0.0));			// just in case autopilot doesn't...
						// actually this breaks last timestep slightly in non-relative target cases
			m_ship->AIMatchAngVelObjSpace(vector3d(0.0));
			if (m_ship->GetFrame()->IsRotFrame()) SetFlightControlState(FlightControlState::CONTROL_FIXSPEED);
			else SetFlightControlState(FlightControlState::CONTROL_MANUAL);
			m_setSpeed = 0.0;
			break;

		case FlightControlState::CONTROL_SLICE:
			PollControls(timeStep, true);
			if (m_ship->GetLaunchLockTimeout() <= 0.0f) {
				if (m_ship->GetSliceDriveState() == Slice::DriveState::DRIVE_READY) {
					// READY
					// Ship.cpp will start the transit drive
				} else if (m_ship->GetSliceDriveState() == Slice::DriveState::DRIVE_START) {
					// START
					// Ship.cpp will engage the transit drive to ON state
				} else if (m_ship->GetSliceDriveState() == Slice::DriveState::DRIVE_ON) {
					double distanceFromBody = -1.0;
					const Frame* frame = m_ship->GetFrame();
					vector3d ship_position = m_ship->GetPositionRelTo(frame);
					vector3d body_position = frame->GetBody()->GetPosition();
					vector3d ship_to_planet = body_position - ship_position;
					distanceFromBody = ship_to_planet.Length() - frame->GetBody()->GetPhysRadius();

					Slice::RSPVector rsp;
					Slice::BodyMinRanges(rsp);
					bool DisengageSliceDrive = true;
					for ( auto &it : rsp ) {
						if ((distanceFromBody < 0.0 || distanceFromBody > it.first)) {
							m_setSpeed = it.second;
							if(m_ship->GetVelocity().Length() != m_setSpeed) {
								m_ship->SetVelocity(-m_ship->GetOrient().VectorZ() * m_setSpeed);
							}
							DisengageSliceDrive = false;
							break;
						}
					}
					if (DisengageSliceDrive) {
						m_ship->DisengageSliceDrive();
						SetFlightControlState(FlightControlState::CONTROL_MANUAL);
					}

					v = -m_ship->GetOrient().VectorZ() * m_setSpeed;
					if (m_setSpeedTarget) {
						v += m_setSpeedTarget->GetVelocityRelTo(m_ship->GetFrame());
					}
					m_ship->AIMatchVel(v);

					// No thrust if ship is at max transit speed, otherwise due to thrust limiter jitter will occur
					double current_velocity = current_velocity = m_ship->GetVelocity().Length();
					if(current_velocity >= m_setSpeed || current_velocity <= -m_setSpeed) {
						v = vector3d(0.0, 0.0, 0.0);
						m_ship->SetVelocity(-m_ship->GetOrient().VectorZ() * m_setSpeed);
					}
					//TransitTunnelingTest(timeStep);
					//TransitStationCatch(timeStep);
				} else if(m_ship->GetSliceDriveState() == Slice::DriveState::DRIVE_STOP) {
					// STOP
				} else if(m_ship->GetSliceDriveState() == Slice::DriveState::DRIVE_OFF) {
					// OFF
				}
			}
			break;
		default: assert(0); break;
		}
	}
	else SetFlightControlState(FlightControlState::CONTROL_MANUAL);

	//call autopilot AI, if active (also applies to set speed and heading lock modes)
	OS::EnableFPE();
	m_ship->AITimeStep(timeStep);
	OS::DisableFPE();
}

void PlayerShipController::CheckControlsLock()
{
	m_controlsLocked = (Pi::game->IsPaused())
		|| Pi::player->IsDead()
		|| (m_ship->GetFlightState() != Ship::FLYING)
		|| (Pi::GetView() != Pi::worldView); //to prevent moving the ship in starmap etc.
}

// mouse wraparound control function
static double clipmouse(double cur, double inp)
{
	if (cur*cur > 0.7 && cur*inp > 0) return 0.0;
	if (inp > 0.2) return 0.2;
	if (inp < -0.2) return -0.2;
	return inp;
}

void PlayerShipController::PollControls(const float timeStep, const bool force_rotation_damping)
{
	static bool stickySpeedKey = false;

	CheckControlsLock();
	if (m_controlsLocked) return;

	// if flying
	{
		m_ship->ClearThrusterState();

		vector3d wantAngVel(0.0);
		double angThrustSoftness = 10.0;

		const float linearThrustPower = (KeyBindings::thrustLowPower.IsActive() ? m_lowThrustPower : 1.0f);

		// have to use this function. SDL mouse position event is bugged in windows
		int mouseMotion[2];
		SDL_GetRelativeMouseState (mouseMotion+0, mouseMotion+1);	// call to flush
		if (Pi::MouseButtonState(SDL_BUTTON_RIGHT))
		{
			const matrix3x3d &rot = m_ship->GetOrient();
			if (!m_mouseActive) {
				m_mouseDir = -rot.VectorZ();	// in world space
				m_mouseX = m_mouseY = 0;
				m_mouseActive = true;
			}
			vector3d objDir = m_mouseDir * rot;

			const double radiansPerPixel = 0.00002 * m_fovY;
			const int maxMotion = std::max(abs(mouseMotion[0]), abs(mouseMotion[1]));
			const double accel = Clamp(maxMotion / 4.0, 0.0, 90.0 / m_fovY);

			m_mouseX += mouseMotion[0] * accel * radiansPerPixel;
			double modx = clipmouse(objDir.x, m_mouseX);
			m_mouseX -= modx;

			const bool invertY = (Pi::IsMouseYInvert() ? !m_invertMouse : m_invertMouse);

			m_mouseY += mouseMotion[1] * accel * radiansPerPixel * (invertY ? -1 : 1);
			double mody = clipmouse(objDir.y, m_mouseY);
			m_mouseY -= mody;

			if(!is_zero_general(modx) || !is_zero_general(mody)) {
				matrix3x3d mrot = matrix3x3d::RotateY(modx) * matrix3x3d::RotateX(mody);
				m_mouseDir = (rot * (mrot * objDir)).Normalized();
			}
		}
		else m_mouseActive = false;

		if (m_flightControlState == FlightControlState::CONTROL_FIXSPEED) {
			double oldSpeed = m_setSpeed;
			if (stickySpeedKey) {
				if (!(KeyBindings::increaseSpeed.IsActive() || KeyBindings::decreaseSpeed.IsActive())) {
					stickySpeedKey = false;
				}
			}

			if (!stickySpeedKey) {
				if (KeyBindings::increaseSpeed.IsActive())
					m_setSpeed += std::max(fabs(m_setSpeed)*0.05, 1.0);
				if (KeyBindings::decreaseSpeed.IsActive())
					m_setSpeed -= std::max(fabs(m_setSpeed)*0.05, 1.0);
				if ( ((oldSpeed < 0.0) && (m_setSpeed >= 0.0)) ||
						((oldSpeed > 0.0) && (m_setSpeed <= 0.0)) ) {
					// flipped from going forward to backwards. make the speed 'stick' at zero
					// until the player lets go of the key and presses it again
					stickySpeedKey = true;
					m_setSpeed = 0;
				}
			}
		}

		if (KeyBindings::thrustForward.IsActive()) m_ship->SetThrusterState(2, -linearThrustPower);
		if (KeyBindings::thrustBackwards.IsActive()) m_ship->SetThrusterState(2, linearThrustPower);
		if (KeyBindings::thrustUp.IsActive()) m_ship->SetThrusterState(1, linearThrustPower);
		if (KeyBindings::thrustDown.IsActive()) m_ship->SetThrusterState(1, -linearThrustPower);
		if (KeyBindings::thrustLeft.IsActive()) m_ship->SetThrusterState(0, -linearThrustPower);
		if (KeyBindings::thrustRight.IsActive()) m_ship->SetThrusterState(0, linearThrustPower);

		if (KeyBindings::yawLeft.IsActive()) wantAngVel.y += 1.0;
		if (KeyBindings::yawRight.IsActive()) wantAngVel.y += -1.0;
		if (KeyBindings::pitchDown.IsActive()) wantAngVel.x += -1.0;
		if (KeyBindings::pitchUp.IsActive()) wantAngVel.x += 1.0;
		if (KeyBindings::rollLeft.IsActive()) wantAngVel.z += 1.0;
		if (KeyBindings::rollRight.IsActive()) wantAngVel.z -= 1.0;

		if (KeyBindings::thrustLowPower.IsActive())
			angThrustSoftness = 50.0;

		vector3d changeVec;
		changeVec.x = KeyBindings::pitchAxis.GetValue();
		changeVec.y = KeyBindings::yawAxis.GetValue();
		changeVec.z = KeyBindings::rollAxis.GetValue();

		// Deadzone more accurate
		for (int axis=0; axis<3; axis++) {
				if (fabs(changeVec[axis]) < m_joystickDeadzone)
					changeVec[axis]=0.0;
				else
					changeVec[axis] = changeVec[axis] * 2.0;
		}
		
		wantAngVel += changeVec;

		if (wantAngVel.Length() >= 0.001 || force_rotation_damping || m_rotationDamping) {
			if (Pi::game->GetTimeAccel()!=Game::TIMEACCEL_1X) {
				for (int axis=0; axis<3; axis++)
					wantAngVel[axis] = wantAngVel[axis] * Pi::game->GetInvTimeAccelRate();
			}

			m_ship->AIModelCoordsMatchAngVel(wantAngVel, angThrustSoftness);
		}

		if (m_mouseActive) m_ship->AIFaceDirection(m_mouseDir);

	}
}

bool PlayerShipController::IsAnyAngularThrusterKeyDown()
{
	return (
		KeyBindings::pitchUp.IsActive()   ||
		KeyBindings::pitchDown.IsActive() ||
		KeyBindings::yawLeft.IsActive()   ||
		KeyBindings::yawRight.IsActive()  ||
		KeyBindings::rollLeft.IsActive()  ||
		KeyBindings::rollRight.IsActive()
	);
}

bool PlayerShipController::IsAnyLinearThrusterKeyDown()
{
	return (
		KeyBindings::thrustForward.IsActive()	||
		KeyBindings::thrustBackwards.IsActive()	||
		KeyBindings::thrustUp.IsActive()		||
		KeyBindings::thrustDown.IsActive()		||
		KeyBindings::thrustLeft.IsActive()		||
		KeyBindings::thrustRight.IsActive()
	);
}

void PlayerShipController::SetFlightControlState(const FlightControlState s)
{
	if (m_flightControlState != s) {
		// finalizer
		switch(m_flightControlState) {
		case FlightControlState::CONTROL_SLICE:
				m_ship->DisengageSliceDrive();
				break;
		}
		// new state
		m_flightControlState = s;
		m_ship->AIClearInstructions();
		switch(m_flightControlState) {
			case FlightControlState::CONTROL_MANUAL: {
				//set desired velocity to current actual
				// Speed is set to the projection of the velocity onto the target.
				vector3d shipVel = m_setSpeedTarget ?
					// Ship's velocity with respect to the target, in current frame's coordinates
					-m_setSpeedTarget->GetVelocityRelTo(m_ship) :
					// Ship's velocity with respect to current frame
					m_ship->GetVelocity();
				// A change from Manual to Set Speed never sets a negative speed.
				m_setSpeed = std::max(shipVel.Dot(-m_ship->GetOrient().VectorZ()), 0.0);
				break;
			}

			case FlightControlState::CONTROL_SLICE:
				m_ship->EngageSliceDrive();
				// Set transit speed to default, limit will be raised in update function based on altitude
				m_setSpeed = Slice::EngageDriveMinSpeed();
				break;
		}
		//XXX global stuff
		Pi::onPlayerChangeFlightControlState.emit();
	}
}

void PlayerShipController::SetLowThrustPower(float power)
{
	assert((power >= 0.0f) && (power <= 1.0f));
	m_lowThrustPower = power;
}

void PlayerShipController::SetRotationDamping(bool enabled)
{
	if (enabled != m_rotationDamping) {
		m_rotationDamping = enabled;
		onRotationDampingChanged.emit();
	}
}

void PlayerShipController::ToggleRotationDamping()
{
	SetRotationDamping(!GetRotationDamping());
}

Body *PlayerShipController::GetNavTarget() const
{
	return m_navTarget;
}

Body *PlayerShipController::GetSetSpeedTarget() const
{
	return m_setSpeedTarget;
}

void PlayerShipController::SetNavTarget(Body* const target, bool setSpeedTo)
{
	if (setSpeedTo)
		m_setSpeedTarget = target;
	else if (m_setSpeedTarget == m_navTarget)
		m_setSpeedTarget = 0;
	m_navTarget = target;
}
