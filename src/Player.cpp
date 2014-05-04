// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Player.h"
#include "Frame.h"
#include "Game.h"
#include "KeyBindings.h"
#include "Lang.h"
#include "Pi.h"
#include "SectorView.h"
#include "Serializer.h"
#include "Sound.h"
#include "SpaceStation.h"
#include "WorldView.h"
#include "StringF.h"

//Some player specific sounds
static Sound::Event s_soundUndercarriage;
static Sound::Event s_soundHyperdrive;

Player::Player(const std::string &shipId): Ship(shipId)
{
	SetController(new PlayerShipController());
	InitCockpit();
}

void Player::Save(Serializer::Writer &wr, Space *space)
{
	Ship::Save(wr, space);
}

void Player::Load(Serializer::Reader &rd, Space *space)
{
	Pi::player = this;
	Ship::Load(rd, space);
	InitCockpit();
}

void Player::InitCockpit()
{
	m_cockpit.release();
	if (!Pi::config->Int("EnableCockpit"))
		return;

	// XXX select a cockpit model. this is all quite skanky because we want a
	// fallback if the name is not found, which means having to actually try to
	// load the model. but ModelBody (on which ShipCockpit is currently based)
	// requires a model name, not a model object. it won't hurt much because it
	// all stays in the model cache anyway, its just awkward. the fix is to fix
	// ShipCockpit so its not a ModelBody and thus does its model work
	// directly, but we're not there yet
	std::string cockpitModel;
	if (!GetShipType()->cockpitModel.empty()) {
		if (Pi::FindModel(GetShipType()->cockpitModel, false))
			cockpitModel = GetShipType()->cockpitModel;
	}
	if (cockpitModel.empty()) {
		if (Pi::FindModel("default_cockpit", false))
			cockpitModel = "default_cockpit";
	}
	if (!cockpitModel.empty())
		m_cockpit.reset(new ShipCockpit(cockpitModel));
}

//XXX perhaps remove this, the sound is very annoying
bool Player::OnDamage(Object *attacker, float kgDamage, const CollisionContact& contactData)
{
	bool r = Ship::OnDamage(attacker, kgDamage, contactData);
	if (!IsDead() && (GetPercentHull() < 25.0f)) {
		Sound::BodyMakeNoise(this, "warning", .5f);
	}
	return r;
}

//XXX handle killcounts in lua
void Player::SetDockedWith(SpaceStation *s, int port)
{
	Ship::SetDockedWith(s, port);
}

//XXX all ships should make this sound
bool Player::SetWheelState(bool down)
{
	bool did = Ship::SetWheelState(down);
	if (did) {
		s_soundUndercarriage.Play(down ? "UC_out" : "UC_in", 1.0f, 1.0f, 0);
	}
	return did;
}

void Player::NotifyRemoved(const Body* const removedBody)
{
	if (GetNavTarget() == removedBody)
		SetNavTarget(0);

	Ship::NotifyRemoved(removedBody);
}

//XXX ui stuff
void Player::OnEnterHyperspace()
{
	s_soundHyperdrive.Play("Hyperdrive_Jump");
	SetNavTarget(0);

	Pi::worldView->HideTargetActions(); // hide the comms menu
	m_controller->SetFlightControlState(FlightControlState::CONTROL_MANUAL); //could set CONTROL_HYPERDRIVE
	ClearThrusterState();
	Pi::game->WantHyperspace();
}

void Player::OnEnterSystem()
{
	m_controller->SetFlightControlState(FlightControlState::CONTROL_MANUAL);
	//XXX don't call sectorview from here, use signals instead
	Pi::sectorView->ResetHyperspaceTarget();
}

//temporary targeting stuff
PlayerShipController *Player::GetPlayerController() const
{
	return static_cast<PlayerShipController*>(GetController());
}

Body *Player::GetNavTarget() const
{
	return static_cast<PlayerShipController*>(m_controller)->GetNavTarget();
}

Body *Player::GetSetSpeedTarget() const
{
	return static_cast<PlayerShipController*>(m_controller)->GetSetSpeedTarget();
}

void Player::SetNavTarget(Body* const target, bool setSpeedTo)
{
	static_cast<PlayerShipController*>(m_controller)->SetNavTarget(target, setSpeedTo);
	Pi::onPlayerChangeTarget.emit();
}
//temporary targeting stuff ends

Ship::HyperjumpStatus Player::InitiateHyperjumpTo(const SystemPath &dest, int warmup_time, double duration) {
	HyperjumpStatus status = Ship::InitiateHyperjumpTo(dest, warmup_time, duration);

	if (status == HYPERJUMP_OK)
		s_soundHyperdrive.Play("Hyperdrive_Charge");

	return status;
}

Ship::HyperjumpStatus Player::StartHyperspaceCountdown(const SystemPath &dest)
{
	HyperjumpStatus status = Ship::StartHyperspaceCountdown(dest);

	if (status == HYPERJUMP_OK)
		s_soundHyperdrive.Play("Hyperdrive_Charge");

	return status;
}

void Player::AbortHyperjump()
{
	s_soundHyperdrive.Play("Hyperdrive_Abort");
	Ship::AbortHyperjump();
}

void Player::ResetHyperspaceCountdown()
{
	s_soundHyperdrive.Play("Hyperdrive_Abort");
	Ship::ResetHyperspaceCountdown();
}

void Player::OnCockpitActivated()
{
	if (m_cockpit)
		m_cockpit->OnActivated();
}

void Player::StaticUpdate(const float timeStep)
{
	Ship::StaticUpdate(timeStep);

	// XXX even when not on screen. hacky, but really cockpit shouldn't be here
	// anyway so this will do for now
	if (m_cockpit)
		m_cockpit->Update(timeStep);
}
