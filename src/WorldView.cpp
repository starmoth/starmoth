// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "WorldView.h"
#include "Pi.h"
#include "Frame.h"
#include "Player.h"
#include "Planet.h"
#include "galaxy/Sector.h"
#include "galaxy/GalaxyCache.h"
#include "SectorView.h"
#include "Serializer.h"
#include "Space.h"
#include "SpaceStation.h"
#include "galaxy/StarSystem.h"
#include "HyperspaceCloud.h"
#include "KeyBindings.h"
#include "perlin.h"
#include "Lang.h"
#include "StringF.h"
#include "Game.h"
#include "graphics/Graphics.h"
#include "graphics/Renderer.h"
#include "graphics/Frustum.h"
#include "graphics/TextureBuilder.h"
#include "graphics/Drawables.h"
#include "matrix4x4.h"
#include "Quaternion.h"
#include <algorithm>
#include <sstream>
#include <SDL_stdinc.h>

const double WorldView::PICK_OBJECT_RECT_SIZE = 20.0;
static const Color s_hudTextColor(0,255,0,230);
static const float ZOOM_SPEED = 1.f;
static const float WHEEL_SENSITIVITY = .05f;	// Should be a variable in user settings.

static const float HUD_CROSSHAIR_SIZE = 24.0f;
static const Uint8 HUD_ALPHA          = 87;

WorldView::WorldView(): UIView()
{
	m_camType = CamType::CAM_INTERNAL;
	InitObject();
}

WorldView::WorldView(Serializer::Reader &rd): UIView()
{
	m_camType = CamType(rd.Int32());
	InitObject();
	m_internalCameraController->Load(rd);
	m_externalCameraController->Load(rd);
	m_siderealCameraController->Load(rd);
}

static const float LOW_THRUST_LEVELS[] = { 0.75, 0.5, 0.25, 0.1, 0.05, 0.01 };

void WorldView::InitObject()
{
	float size[2];
	GetSizeRequested(size);

	m_showTargetActionsTimeout = 0;
	m_showCameraNameTimeout = 0;
	m_showCameraName = 0;
	m_labelsOn = true;
	SetTransparency(true);

	Graphics::RenderStateDesc rsd;
	rsd.blendMode = Graphics::BLEND_ALPHA;
	rsd.depthWrite = false;
	rsd.depthTest = false;
	m_blendState = Pi::renderer->CreateRenderState(rsd); //XXX m_renderer not set yet
	m_navTunnel = new NavTunnelWidget(this, m_blendState);
	Add(m_navTunnel, 0, 0);

	m_commsOptions = new Fixed(size[0], size[1]/2);
	m_commsOptions->SetTransparency(true);
	Add(m_commsOptions, 10, 200);

	m_commsNavOptionsContainer = new Gui::HBox();
	m_commsNavOptionsContainer->SetSpacing(5);
	m_commsNavOptionsContainer->SetSizeRequest(220, size[1]-50);
	Add(m_commsNavOptionsContainer, size[0]-230, 20);

	Gui::VScrollPortal *portal = new Gui::VScrollPortal(200);
	Gui::VScrollBar *scroll = new Gui::VScrollBar();
	scroll->SetAdjustment(&portal->vscrollAdjust);
	m_commsNavOptionsContainer->PackStart(scroll);
	m_commsNavOptionsContainer->PackStart(portal);

	m_commsNavOptions = new Gui::VBox();
	m_commsNavOptions->SetSpacing(5);
	portal->Add(m_commsNavOptions);

#if WITH_DEVKEYS
	Gui::Screen::PushFont("ConsoleFont");
	m_debugInfo = (new Gui::Label(""))->Color(204, 204, 204);
	Add(m_debugInfo, 10, 200);
	Gui::Screen::PopFont();
#endif

	m_hudHyperspaceInfo = (new Gui::Label(""))->Color(s_hudTextColor);
	Add(m_hudHyperspaceInfo, Gui::Screen::GetWidth()*0.4f, Gui::Screen::GetHeight()*0.3f);

	m_hudHullTemp = new Gui::MeterBar(100.0f, Lang::HULL_TEMP, Color(255,0,0,204));
	m_hudHullIntegrity = new Gui::MeterBar(100.0f, Lang::HULL_INTEGRITY, Color(255,255,0,204));
	Add(m_hudHullTemp, 5.0f, Gui::Screen::GetHeight() - 144.0f);
	Add(m_hudHullIntegrity, Gui::Screen::GetWidth() - 105.0f, Gui::Screen::GetHeight() - 104.0f);

	m_hudTargetHullIntegrity = new Gui::MeterBar(100.0f, Lang::HULL_INTEGRITY, Color(255,255,0,204));
	Add(m_hudTargetHullIntegrity, Gui::Screen::GetWidth() - 105.0f, 5.0f);

	m_hudTargetInfo = (new Gui::Label(""))->Color(s_hudTextColor);
	Add(m_hudTargetInfo, 0, 85.0f);

	Gui::Screen::PushFont("OverlayFont");
	m_bodyLabels = new Gui::LabelSet();
	m_bodyLabels->SetLabelColor(Color(255, 255, 255, 230));
	Add(m_bodyLabels, 0, 0);

	{
		m_pauseText = new Gui::Label(std::string("#f7f") + Lang::PAUSED);
		float w, h;
		Gui::Screen::MeasureString(Lang::PAUSED, w, h);
		Add(m_pauseText, 0.5f * (Gui::Screen::GetWidth() - w), 100);
	}
	Gui::Screen::PopFont();

	m_navTargetIndicator.label = (new Gui::Label(""))->Color(0, 255, 0);
	m_navVelIndicator.label = (new Gui::Label(""))->Color(0, 255, 0);

	// these labels are repositioned during Draw3D()
	Add(m_navTargetIndicator.label, 0, 0);
	Add(m_navVelIndicator.label, 0, 0);

	// XXX m_renderer not set yet
	Graphics::TextureBuilder b = Graphics::TextureBuilder::UI("icons/indicator_mousedir.png");
	m_indicatorMousedir.reset(new Gui::TexturedQuad(b.GetOrCreateTexture(Gui::Screen::GetRenderer(), "ui")));

	const Graphics::TextureDescriptor &descriptor = b.GetDescriptor();
	m_indicatorMousedirSize = vector2f(descriptor.dataSize.x*descriptor.texSize.x,descriptor.dataSize.y*descriptor.texSize.y);

    m_speedLines.reset(new SpeedLines(Pi::player));

	//get near & far clipping distances
	//XXX m_renderer not set yet
	float znear;
	float zfar;
	Pi::renderer->GetNearFarRange(znear, zfar);

	const float fovY = Pi::config->Float("FOVVertical");

	m_cameraContext.Reset(new CameraContext(Graphics::GetScreenWidth(), Graphics::GetScreenHeight(), fovY, znear, zfar));
	m_camera.reset(new Camera(m_cameraContext, Pi::renderer));
	m_internalCameraController.reset(new InternalCameraController(m_cameraContext, Pi::player));
	m_externalCameraController.reset(new ExternalCameraController(m_cameraContext, Pi::player));
	m_siderealCameraController.reset(new SiderealCameraController(m_cameraContext, Pi::player));
	SetCamType(m_camType); //set the active camera

	m_onHyperspaceTargetChangedCon =
		Pi::sectorView->onHyperspaceTargetChanged.connect(sigc::mem_fun(this, &WorldView::OnHyperspaceTargetChanged));

	m_onPlayerChangeTargetCon =
		Pi::onPlayerChangeTarget.connect(sigc::mem_fun(this, &WorldView::OnPlayerChangeTarget));
	m_onMouseWheelCon =
		Pi::onMouseWheel.connect(sigc::mem_fun(this, &WorldView::MouseWheel));

	Pi::player->GetPlayerController()->SetMouseForRearView(GetCamType() == CamType::CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);
	KeyBindings::toggleHudMode.onPress.connect(sigc::mem_fun(this, &WorldView::OnToggleLabels));
}

WorldView::~WorldView()
{
	m_onHyperspaceTargetChangedCon.disconnect();
	m_onPlayerChangeTargetCon.disconnect();
	m_onChangeFlightControlStateCon.disconnect();
	m_onMouseWheelCon.disconnect();
}

void WorldView::Save(Serializer::Writer &wr)
{
	wr.Int32(int(m_camType));
	m_internalCameraController->Save(wr);
	m_externalCameraController->Save(wr);
	m_siderealCameraController->Save(wr);
}

void WorldView::SetCamType(CamType c)
{
	// don't allow external cameras when docked inside space stations.
	// they would clip through the station model
	if (Pi::player->GetFlightState() == Ship::DOCKED && !Pi::player->GetDockedWith()->IsGroundStation())
		c = CamType::CAM_INTERNAL;

	m_camType = c;

	switch(m_camType) {
		case CamType::CAM_INTERNAL:
			m_activeCameraController = m_internalCameraController.get();
			Pi::player->OnCockpitActivated();
			break;
		case CamType::CAM_EXTERNAL:
			m_activeCameraController = m_externalCameraController.get();
			break;
		case CamType::CAM_SIDEREAL:
			m_activeCameraController = m_siderealCameraController.get();
			break;
	}

	Pi::player->GetPlayerController()->SetMouseForRearView(m_camType == CamType::CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);

	m_activeCameraController->Reset();

	onChangeCamType.emit();

	UpdateCameraName();
}

void WorldView::ChangeInternalCameraMode(InternalCameraController::Mode m)
{
	if (m_internalCameraController->GetMode() == m) return;

	m_internalCameraController->SetMode(m);
	Pi::player->GetPlayerController()->SetMouseForRearView(m_camType == CamType::CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR);
	UpdateCameraName();
}

void WorldView::UpdateCameraName()
{
	if (m_showCameraName)
		Remove(m_showCameraName);

	const std::string cameraName(m_activeCameraController->GetName());

	Gui::Screen::PushFont("OverlayFont");
	m_showCameraName = new Gui::Label("#ff0"+cameraName);
	Gui::Screen::PopFont();

	float w, h;
	Gui::Screen::MeasureString(cameraName, w, h);
	Add(m_showCameraName, 0.5f*(Gui::Screen::GetWidth()-w), 20);

	m_showCameraNameTimeout = SDL_GetTicks();
}

void WorldView::Draw3D()
{
	PROFILE_SCOPED()
	assert(Pi::game);
	assert(Pi::player);
	assert(!Pi::player->IsDead());

	m_cameraContext->ApplyDrawTransforms(m_renderer);

	Body* excludeBody = nullptr;
	ShipCockpit* cockpit = nullptr;
	if(GetCamType() == CamType::CAM_INTERNAL) {
		excludeBody = Pi::player;
		if (m_internalCameraController->GetMode() == InternalCameraController::MODE_FRONT)
			cockpit = Pi::player->GetCockpit();
	}
	m_camera->Draw(excludeBody, cockpit);

	// Draw 3D HUD
	// Speed lines
	if (Pi::AreSpeedLinesDisplayed())
		m_speedLines->Render(m_renderer);

	// Contact trails
	if( Pi::AreHudTrailsDisplayed() ) {
		for (auto it = Pi::player->GetSensors()->GetContacts().begin(); it != Pi::player->GetSensors()->GetContacts().end(); ++it)
			it->trail->Render(m_renderer);
	}

	m_cameraContext->EndFrame();

	UIView::Draw3D();
}

void WorldView::OnToggleLabels()
{
	if (Pi::GetView() == this) {
		if (Pi::DrawGUI && m_labelsOn) {
			m_labelsOn = false;
		} else if (Pi::DrawGUI && !m_labelsOn) {
			Pi::DrawGUI = false;
		} else if (!Pi::DrawGUI) {
			Pi::DrawGUI = true;
			m_labelsOn = true;
		}
	}
}

void WorldView::ShowAll()
{
	View::ShowAll(); // by default, just delegate back to View
	RefreshButtonStateAndVisibility();
}

static Color get_color_for_warning_meter_bar(float v) {
	Color c;
	if (v < 50.0f)
		c = Color(255,0,0,HUD_ALPHA);
	else if (v < 75.0f)
		c = Color(255,128,0,HUD_ALPHA);
	else
		c = Color(255,255,0,HUD_ALPHA);
	return c;
}

void WorldView::RefreshButtonStateAndVisibility()
{
	assert(Pi::game);
	assert(Pi::player);
	assert(!Pi::player->IsDead());

	if (Pi::game->IsPaused())
		m_pauseText->Show();
	else
		m_pauseText->Hide();

	// Direction indicator
	if (m_showTargetActionsTimeout) {
		if (SDL_GetTicks() - m_showTargetActionsTimeout > 20000) {
			m_showTargetActionsTimeout = 0;
			m_commsOptions->DeleteAllChildren();
			m_commsNavOptions->DeleteAllChildren();
		}
		m_commsOptions->ShowAll();
		m_commsNavOptionsContainer->ShowAll();
	} else {
		m_commsOptions->Hide();
		m_commsNavOptionsContainer->Hide();
	}

#if WITH_DEVKEYS
	if (Pi::showDebugInfo) {
		std::ostringstream ss;

		if (Pi::player->GetFlightState() != Ship::HYPERSPACE) {
			vector3d pos = Pi::player->GetPosition();
			vector3d abs_pos = Pi::player->GetPositionRelTo(Pi::game->GetSpace()->GetRootFrame());

			ss << stringf("Pos: %0{f.2}, %1{f.2}, %2{f.2}\n", pos.x, pos.y, pos.z);
			ss << stringf("AbsPos: %0{f.2}, %1{f.2}, %2{f.2}\n", abs_pos.x, abs_pos.y, abs_pos.z);

			const SystemPath &path(Pi::player->GetFrame()->GetSystemBody()->GetPath());
			ss << stringf("Rel-to: %0 [%1{d},%2{d},%3{d},%4{u},%5{u}] ",
				Pi::player->GetFrame()->GetLabel(),
				path.sectorX, path.sectorY, path.sectorZ, path.systemIndex, path.bodyIndex);
			ss << stringf("(%0{f.2} km), rotating: %1\n",
				pos.Length()/1000, (Pi::player->GetFrame()->IsRotFrame() ? "yes" : "no"));

			//Calculate lat/lon for ship position
			const vector3d dir = pos.NormalizedSafe();
			const float lat = RAD2DEG(asin(dir.y));
			const float lon = RAD2DEG(atan2(dir.x, dir.z));

			ss << stringf("Lat / Lon: %0{f.8} / %1{f.8}\n", lat, lon);
		}

		char aibuf[256];
		Pi::player->AIGetStatusText(aibuf); aibuf[255] = 0;
		ss << aibuf << std::endl;

		m_debugInfo->SetText(ss.str());
		m_debugInfo->Show();
	} else {
		m_debugInfo->Hide();
	}
#endif

	if (Pi::player->GetHullTemperature() > 0.01) {
		m_hudHullTemp->SetValue(float(Pi::player->GetHullTemperature()));
		m_hudHullTemp->Show();
	} else {
		m_hudHullTemp->Hide();
	}

	float hull = Pi::player->GetPercentHull();
	if (hull < 100.0f) {
		m_hudHullIntegrity->SetColor(get_color_for_warning_meter_bar(hull));
		m_hudHullIntegrity->SetValue(hull*0.01f);
		m_hudHullIntegrity->Show();
	} else {
		m_hudHullIntegrity->Hide();
	}

	if (Pi::player->IsHyperspaceActive()) {
		float val = Pi::player->GetHyperspaceCountdown();

		if (!(int(ceil(val*2.0)) % 2)) {
			m_hudHyperspaceInfo->SetText(stringf(Lang::HYPERSPACING_IN_N_SECONDS, formatarg("countdown", ceil(val))));
			m_hudHyperspaceInfo->Show();
		} else {
			m_hudHyperspaceInfo->Hide();
		}
	} else {
		m_hudHyperspaceInfo->Hide();
	}
}

void WorldView::Update()
{
	PROFILE_SCOPED()
	assert(Pi::game);
	assert(Pi::player);
	assert(!Pi::player->IsDead());

	const double frameTime = Pi::GetFrameTime();
	// show state-appropriate buttons
	RefreshButtonStateAndVisibility();

	if (Pi::MouseButtonState(SDL_BUTTON_RIGHT)) {
		// when controlling your ship with the mouse you don't want to pick targets
		m_bodyLabels->SetLabelsClickable(false);
	} else {
		m_bodyLabels->SetLabelsClickable(true);
	}

	m_bodyLabels->SetLabelsVisible(m_labelsOn);

	bool targetObject = false;

	if (GetCamType() == CamType::CAM_INTERNAL) {
		if      (KeyBindings::frontCamera.IsActive())  ChangeInternalCameraMode(InternalCameraController::MODE_FRONT);
		else if (KeyBindings::rearCamera.IsActive())   ChangeInternalCameraMode(InternalCameraController::MODE_REAR);
		else if (KeyBindings::leftCamera.IsActive())   ChangeInternalCameraMode(InternalCameraController::MODE_LEFT);
		else if (KeyBindings::rightCamera.IsActive())  ChangeInternalCameraMode(InternalCameraController::MODE_RIGHT);
		else if (KeyBindings::topCamera.IsActive())    ChangeInternalCameraMode(InternalCameraController::MODE_TOP);
		else if (KeyBindings::bottomCamera.IsActive()) ChangeInternalCameraMode(InternalCameraController::MODE_BOTTOM);
	}
	else {
		MoveableCameraController *cam = static_cast<MoveableCameraController*>(m_activeCameraController);
		if (KeyBindings::cameraRotateUp.IsActive()) cam->RotateUp(frameTime);
		if (KeyBindings::cameraRotateDown.IsActive()) cam->RotateDown(frameTime);
		if (KeyBindings::cameraRotateLeft.IsActive()) cam->RotateLeft(frameTime);
		if (KeyBindings::cameraRotateRight.IsActive()) cam->RotateRight(frameTime);
		if (KeyBindings::viewZoomOut.IsActive()) cam->ZoomEvent(ZOOM_SPEED*frameTime);		// Zoom out
		if (KeyBindings::viewZoomIn.IsActive()) cam->ZoomEvent(-ZOOM_SPEED*frameTime);
		if (KeyBindings::cameraRollLeft.IsActive()) cam->RollLeft(frameTime);
		if (KeyBindings::cameraRollRight.IsActive()) cam->RollRight(frameTime);
		if (KeyBindings::resetCamera.IsActive()) cam->Reset();
		cam->ZoomEventUpdate(frameTime);
	}

	// note if we have to target the object in the crosshairs
	targetObject = KeyBindings::targetObject.IsActive();

	if (m_showCameraNameTimeout) {
		if (SDL_GetTicks() - m_showCameraNameTimeout > 20000) {
			m_showCameraName->Hide();
			m_showCameraNameTimeout = 0;
		} else {
			m_showCameraName->Show();
		}
	}

	m_activeCameraController->Update();

	m_cameraContext->BeginFrame();
	m_camera->Update();

	UpdateProjectedObjects();

	const Frame *playerFrame = Pi::player->GetFrame();
	const Frame *camFrame = m_cameraContext->GetCamFrame();

	//speedlines and contact trails need camFrame for transform, so they
	//must be updated here
	if (Pi::AreSpeedLinesDisplayed()) {
		m_speedLines->Update(Pi::game->GetTimeStep());

		matrix4x4d trans;
		Frame::GetFrameTransform(playerFrame, camFrame, trans);

		if ( m_speedLines.get() && Pi::AreSpeedLinesDisplayed() ) {
			m_speedLines->Update(Pi::game->GetTimeStep());

			trans[12] = trans[13] = trans[14] = 0.0;
			trans[15] = 1.0;
			m_speedLines->SetTransform(trans);
		}
	}

	if( Pi::AreHudTrailsDisplayed() )
	{
		matrix4x4d trans;
		Frame::GetFrameTransform(playerFrame, camFrame, trans);

		for (auto it = Pi::player->GetSensors()->GetContacts().begin(); it != Pi::player->GetSensors()->GetContacts().end(); ++it)
			it->trail->SetTransform(trans);
	} else {
		for (auto it = Pi::player->GetSensors()->GetContacts().begin(); it != Pi::player->GetSensors()->GetContacts().end(); ++it)
			it->trail->Reset(playerFrame);
	}

	// target object under the crosshairs. must be done after
	// UpdateProjectedObjects() to be sure that m_projectedPos does not have
	// contain references to deleted objects
	if (targetObject) {
		Body* const target = PickBody(double(Gui::Screen::GetWidth())/2.0, double(Gui::Screen::GetHeight())/2.0);
		SelectBody(target, false);
	}

	UIView::Update();
}

void WorldView::OnSwitchTo()
{
	UIView::OnSwitchTo();
	RefreshButtonStateAndVisibility();
}

void WorldView::OnSwitchFrom()
{
	Pi::DrawGUI = true;
}

void WorldView::ToggleTargetActions()
{
	if (Pi::game->IsHyperspace() || m_showTargetActionsTimeout)
		HideTargetActions();
	else
		ShowTargetActions();
}

void WorldView::ShowTargetActions()
{
	m_showTargetActionsTimeout = SDL_GetTicks();
	UpdateCommsOptions();
}

void WorldView::HideTargetActions()
{
	m_showTargetActionsTimeout = 0;
	UpdateCommsOptions();
}

Gui::Button *WorldView::AddCommsOption(const std::string &msg, int ypos, int optnum)
{
	Gui::Label *l = new Gui::Label(msg);
	m_commsOptions->Add(l, 50, float(ypos));

	char buf[8];
	snprintf(buf, sizeof(buf), "%d", optnum);
	Gui::LabelButton *b = new Gui::LabelButton(new Gui::Label(buf));
	b->SetShortcut(SDL_Keycode(SDLK_0 + optnum), KMOD_NONE);
	// hide target actions when things get clicked on
	b->onClick.connect(sigc::mem_fun(this, &WorldView::ToggleTargetActions));
	m_commsOptions->Add(b, 16, float(ypos));
	return b;
}

void WorldView::OnClickCommsNavOption(Body *target)
{
	Pi::player->SetNavTarget(target);
	m_showTargetActionsTimeout = SDL_GetTicks();
}

void WorldView::AddCommsNavOption(const std::string &msg, Body *target)
{
	Gui::HBox *hbox = new Gui::HBox();
	hbox->SetSpacing(5);

	Gui::Label *l = new Gui::Label(msg);
	hbox->PackStart(l);

	Gui::Button *b = new Gui::SolidButton();
	b->onClick.connect(sigc::bind(sigc::mem_fun(this, &WorldView::OnClickCommsNavOption), target));
	hbox->PackStart(b);

	m_commsNavOptions->PackEnd(hbox);
}

void WorldView::BuildCommsNavOptions()
{
	std::map< Uint32,std::vector<SystemBody*> > groups;

	m_commsNavOptions->PackEnd(new Gui::Label(std::string("#ff0")+std::string(Lang::NAVIGATION_TARGETS_IN_THIS_SYSTEM)+std::string("\n")));

	for (SystemBody* station : Pi::game->GetSpace()->GetStarSystem()->GetSpaceStations()) {
		groups[station->GetParent()->GetPath().bodyIndex].push_back(station);
	}

	for ( std::map< Uint32,std::vector<SystemBody*> >::const_iterator i = groups.begin(); i != groups.end(); ++i ) {
		m_commsNavOptions->PackEnd(new Gui::Label("#f0f" + Pi::game->GetSpace()->GetStarSystem()->GetBodies()[(*i).first]->GetName()));

		for ( std::vector<SystemBody*>::const_iterator j = (*i).second.begin(); j != (*i).second.end(); ++j) {
			SystemPath path = Pi::game->GetSpace()->GetStarSystem()->GetPathOf(*j);
			Body *body = Pi::game->GetSpace()->FindBodyForPath(&path);
			AddCommsNavOption((*j)->GetName(), body);
		}
	}
}

static void PlayerRequestDockingClearance(SpaceStation *s)
{
	std::string msg;
	s->GetDockingClearance(Pi::player, msg);
}

// XXX belongs in some sort of hyperspace controller
void WorldView::OnHyperspaceTargetChanged()
{
	if (Pi::player->IsHyperspaceActive())
		Pi::player->ResetHyperspaceCountdown();
}

void WorldView::OnPlayerChangeTarget()
{
	Body *b = Pi::player->GetNavTarget();
	if (b) {
		Ship *s = b->IsType(Object::HYPERSPACECLOUD) ? static_cast<HyperspaceCloud*>(b)->GetShip() : 0;
		if (!s || !Pi::sectorView->GetHyperspaceTarget().IsSameSystem(s->GetHyperspaceDest()))
			Pi::sectorView->FloatHyperspaceTarget();
	}

	UpdateCommsOptions();
}

static void autopilot_flyto(Body *b)
{
	Pi::player->GetPlayerController()->SetFlightControlState(FlightControlState::CONTROL_AUTOPILOT);
	Pi::player->AIFlyTo(b);
}
static void autopilot_dock(Body *b)
{
	if(Pi::player->GetFlightState() != Ship::FLYING)
		return;

	Pi::player->GetPlayerController()->SetFlightControlState(FlightControlState::CONTROL_AUTOPILOT);
	Pi::player->AIDock(static_cast<SpaceStation*>(b));
}
static void autopilot_orbit(Body *b, double alt)
{
	Pi::player->GetPlayerController()->SetFlightControlState(FlightControlState::CONTROL_AUTOPILOT);
	Pi::player->AIOrbit(b, alt);
}

void WorldView::UpdateCommsOptions()
{
	m_commsOptions->DeleteAllChildren();
	m_commsNavOptions->DeleteAllChildren();

	if (m_showTargetActionsTimeout == 0) return;

	if (Pi::game->GetSpace()->GetStarSystem()->HasSpaceStations())
	{
		BuildCommsNavOptions();
	}

	Body * const navtarget = Pi::player->GetNavTarget();
	Gui::Button *button;
	int ypos = 0;
	int optnum = 1;
	if (!navtarget)
		m_commsOptions->Add(new Gui::Label("#0f0"+std::string(Lang::NO_TARGET_SELECTED)), 16, float(ypos));

	const bool isFlying = Pi::player->GetFlightState() == Ship::FLYING;

	if (navtarget) {
		m_commsOptions->Add(new Gui::Label("#0f0"+navtarget->GetLabel()), 16, float(ypos));
		ypos += 32;
		if (navtarget->IsType(Object::SPACESTATION)) {
			SpaceStation *pStation = static_cast<SpaceStation *>(navtarget);
			if( pStation->GetMyDockingPort(Pi::player) == -1 )
			{
				button = AddCommsOption(Lang::REQUEST_DOCKING_CLEARANCE, ypos, optnum++);
				button->onClick.connect(sigc::bind(sigc::ptr_fun(&PlayerRequestDockingClearance), static_cast<SpaceStation*>(navtarget)));
				ypos += 32;
			}

			if(isFlying)
			{
				button = AddCommsOption(Lang::AUTOPILOT_DOCK_WITH_STATION, ypos, optnum++);
				button->onClick.connect(sigc::bind(sigc::ptr_fun(&autopilot_dock), navtarget));
				ypos += 32;
			}
		}

		if (isFlying) {
			button = AddCommsOption(stringf(Lang::AUTOPILOT_FLY_TO_VICINITY_OF, formatarg("target", navtarget->GetLabel())), ypos, optnum++);
			button->onClick.connect(sigc::bind(sigc::ptr_fun(&autopilot_flyto), navtarget));
			ypos += 32;

			if (navtarget->IsType(Object::PLANET) || navtarget->IsType(Object::STAR)) {
				button = AddCommsOption(stringf(Lang::AUTOPILOT_ENTER_LOW_ORBIT_AROUND, formatarg("target", navtarget->GetLabel())), ypos, optnum++);
				button->onClick.connect(sigc::bind(sigc::ptr_fun(autopilot_orbit), navtarget, 1.2));
				ypos += 32;

				button = AddCommsOption(stringf(Lang::AUTOPILOT_ENTER_MEDIUM_ORBIT_AROUND, formatarg("target", navtarget->GetLabel())), ypos, optnum++);
				button->onClick.connect(sigc::bind(sigc::ptr_fun(autopilot_orbit), navtarget, 1.6));
				ypos += 32;

				button = AddCommsOption(stringf(Lang::AUTOPILOT_ENTER_HIGH_ORBIT_AROUND, formatarg("target", navtarget->GetLabel())), ypos, optnum++);
				button->onClick.connect(sigc::bind(sigc::ptr_fun(autopilot_orbit), navtarget, 3.2));
				ypos += 32;
			}
		}
	}
}

void WorldView::SelectBody(Body *target, bool reselectIsDeselect)
{
	if (!target || target == Pi::player) return;		// don't select self

	if (Pi::player->GetNavTarget() == target) {
		if (reselectIsDeselect) Pi::player->SetNavTarget(0);
	}
	else
		Pi::player->SetNavTarget(target, Pi::KeyState(SDLK_LCTRL) || Pi::KeyState(SDLK_RCTRL));
}

Body* WorldView::PickBody(const double screenX, const double screenY) const
{
	for (std::map<Body*,vector3d>::const_iterator
		i = m_projectedPos.begin(); i != m_projectedPos.end(); ++i) {
		Body *b = i->first;

		if (b == Pi::player)
			continue;

		const double x1 = i->second.x - PICK_OBJECT_RECT_SIZE * 0.5;
		const double x2 = x1 + PICK_OBJECT_RECT_SIZE;
		const double y1 = i->second.y - PICK_OBJECT_RECT_SIZE * 0.5;
		const double y2 = y1 + PICK_OBJECT_RECT_SIZE;
		if(screenX >= x1 && screenX <= x2 && screenY >= y1 && screenY <= y2)
			return b;
	}

	return 0;
}

static inline bool project_to_screen(const vector3d &in, vector3d &out, const Graphics::Frustum &frustum, const int guiSize[2])
{
	if (!frustum.ProjectPoint(in, out)) return false;
	out.x *= guiSize[0];
	out.y = Gui::Screen::GetHeight() - out.y * guiSize[1];
	return true;
}

void WorldView::UpdateProjectedObjects()
{
	const int guiSize[2] = { Gui::Screen::GetWidth(), Gui::Screen::GetHeight() };
	const Graphics::Frustum frustum = m_cameraContext->GetFrustum();

	const Frame *cam_frame = m_cameraContext->GetCamFrame();
	matrix3x3d cam_rot = cam_frame->GetOrient();

	// determine projected positions and update labels
	m_bodyLabels->Clear();
	m_projectedPos.clear();
	for (Body* b : Pi::game->GetSpace()->GetBodies()) {
		// don't show the player label on internal camera
		if (b->IsType(Object::PLAYER) && GetCamType() == CamType::CAM_INTERNAL)
			continue;

		vector3d pos = b->GetInterpPositionRelTo(cam_frame);
		if ((pos.z < -1.0) && project_to_screen(pos, pos, frustum, guiSize)) {

			// only show labels on large or nearby bodies
			if (b->IsType(Object::PLANET) || b->IsType(Object::STAR) || b->IsType(Object::SPACESTATION) || Pi::player->GetPositionRelTo(b).LengthSqr() < 1000000.0*1000000.0)
				m_bodyLabels->Add(b->GetLabel(), sigc::bind(sigc::mem_fun(this, &WorldView::SelectBody), b, true), float(pos.x), float(pos.y));

			m_projectedPos[b] = pos;
		}
	}

	// velocity relative to current frame (white)
	const vector3d camSpaceVel = Pi::player->GetVelocity() * cam_rot;
	if (camSpaceVel.LengthSqr() >= 1e-4)
		UpdateIndicator(m_velIndicator, camSpaceVel);
	else
		HideIndicator(m_velIndicator);

	// orientation according to mouse
	if (Pi::player->GetPlayerController()->IsMouseActive()) {
		vector3d mouseDir = Pi::player->GetPlayerController()->GetMouseDir() * cam_rot;
		if (GetCamType() == CamType::CAM_INTERNAL && m_internalCameraController->GetMode() == InternalCameraController::MODE_REAR)
			mouseDir = -mouseDir;
		UpdateIndicator(m_mouseDirIndicator, (Pi::player->GetPhysRadius() * 1.5) * mouseDir);
	} else
		HideIndicator(m_mouseDirIndicator);

	// navtarget info
	if (Body *navtarget = Pi::player->GetNavTarget()) {
		// if navtarget and body frame are the same,
		// then we hide the frame-relative velocity indicator
		// (which would be hidden underneath anyway)
		if (navtarget == Pi::player->GetFrame()->GetBody())
			HideIndicator(m_velIndicator);

		// navtarget distance/target square indicator (displayed with navtarget label)
		double dist = (navtarget->GetTargetIndicatorPosition(cam_frame)
			- Pi::player->GetPositionRelTo(cam_frame)).Length();
		m_navTargetIndicator.label->SetText(format_distance(dist).c_str());
		UpdateIndicator(m_navTargetIndicator, navtarget->GetTargetIndicatorPosition(cam_frame));

		// velocity relative to navigation target
		vector3d navvelocity = -navtarget->GetVelocityRelTo(Pi::player);
		double navspeed = navvelocity.Length();
		const vector3d camSpaceNavVel = navvelocity * cam_rot;

		if (navspeed >= 0.01) { // 1 cm per second
			char buf[128];
			if (navspeed > 1000)
				snprintf(buf, sizeof(buf), "%.2f km/s", navspeed*0.001);
			else
				snprintf(buf, sizeof(buf), "%.0f m/s", navspeed);
			m_navVelIndicator.label->SetText(buf);
			UpdateIndicator(m_navVelIndicator, camSpaceNavVel);

			assert(m_navTargetIndicator.side != INDICATOR_HIDDEN);
			assert(m_navVelIndicator.side != INDICATOR_HIDDEN);
			SeparateLabels(m_navTargetIndicator.label, m_navVelIndicator.label);
		} else
			HideIndicator(m_navVelIndicator);

	} else {
		HideIndicator(m_navTargetIndicator);
		HideIndicator(m_navVelIndicator);
	}
}

void WorldView::UpdateIndicator(Indicator &indicator, const vector3d &cameraSpacePos)
{
	const int guiSize[2] = { Gui::Screen::GetWidth(), Gui::Screen::GetHeight() };
	const Graphics::Frustum frustum = m_cameraContext->GetFrustum();

	const float BORDER = 10.0;
	const float BORDER_BOTTOM = 90.0;
	// XXX BORDER_BOTTOM is 10+the control panel height and shouldn't be needed at all

	const float w = Gui::Screen::GetWidth();
	const float h = Gui::Screen::GetHeight();

	if (cameraSpacePos.LengthSqr() < 1e-6) { // length < 1e-3
		indicator.pos.x = w/2.0f;
		indicator.pos.y = h/2.0f;
		indicator.side = INDICATOR_ONSCREEN;
	} else {
		vector3d proj;
		bool success = project_to_screen(cameraSpacePos, proj, frustum, guiSize);
		if (! success)
			proj = vector3d(w/2.0, h/2.0, 0.0);

		indicator.realpos.x = int(proj.x);
		indicator.realpos.y = int(proj.y);

		bool onscreen =
			(cameraSpacePos.z < 0.0) &&
			(proj.x >= BORDER) && (proj.x < w - BORDER) &&
			(proj.y >= BORDER) && (proj.y < h - BORDER_BOTTOM);

		if (onscreen) {
			indicator.pos.x = int(proj.x);
			indicator.pos.y = int(proj.y);
			indicator.side = INDICATOR_ONSCREEN;
		} else {
			// homogeneous 2D points and lines are really useful
			const vector3d ptCentre(w/2.0, h/2.0, 1.0);
			const vector3d ptProj(proj.x, proj.y, 1.0);
			const vector3d lnDir = ptProj.Cross(ptCentre);

			indicator.side = INDICATOR_TOP;

			// this fallback is used if direction is close to (0, 0, +ve)
			indicator.pos.x = w/2.0;
			indicator.pos.y = BORDER;

			if (cameraSpacePos.x < -1e-3) {
				vector3d ptLeft = lnDir.Cross(vector3d(-1.0, 0.0, BORDER));
				ptLeft /= ptLeft.z;
				if (ptLeft.y >= BORDER && ptLeft.y < h - BORDER_BOTTOM) {
					indicator.pos.x = ptLeft.x;
					indicator.pos.y = ptLeft.y;
					indicator.side = INDICATOR_LEFT;
				}
			} else if (cameraSpacePos.x > 1e-3) {
				vector3d ptRight = lnDir.Cross(vector3d(-1.0, 0.0,  w - BORDER));
				ptRight /= ptRight.z;
				if (ptRight.y >= BORDER && ptRight.y < h - BORDER_BOTTOM) {
					indicator.pos.x = ptRight.x;
					indicator.pos.y = ptRight.y;
					indicator.side = INDICATOR_RIGHT;
				}
			}

			if (cameraSpacePos.y < -1e-3) {
				vector3d ptBottom = lnDir.Cross(vector3d(0.0, -1.0, h - BORDER_BOTTOM));
				ptBottom /= ptBottom.z;
				if (ptBottom.x >= BORDER && ptBottom.x < w-BORDER) {
					indicator.pos.x = ptBottom.x;
					indicator.pos.y = ptBottom.y;
					indicator.side = INDICATOR_BOTTOM;
				}
			} else if (cameraSpacePos.y > 1e-3) {
				vector3d ptTop = lnDir.Cross(vector3d(0.0, -1.0, BORDER));
				ptTop /= ptTop.z;
				if (ptTop.x >= BORDER && ptTop.x < w - BORDER) {
					indicator.pos.x = ptTop.x;
					indicator.pos.y = ptTop.y;
					indicator.side = INDICATOR_TOP;
				}
			}
		}
	}

	// update the label position
	if (indicator.label) {
		if (indicator.side != INDICATOR_HIDDEN) {
			float labelSize[2] = { 500.0f, 500.0f };
			indicator.label->GetSizeRequested(labelSize);

			int pos[2] = {0,0};
			switch (indicator.side) {
			case INDICATOR_HIDDEN: break;
			case INDICATOR_ONSCREEN: // when onscreen, default to label-below unless it would clamp to be on top of the marker
				pos[0] = -(labelSize[0]/2.0f);
				if (indicator.pos.y + pos[1] + labelSize[1] + HUD_CROSSHAIR_SIZE + 2.0f > h - BORDER_BOTTOM)
					pos[1] = -(labelSize[1] + HUD_CROSSHAIR_SIZE + 2.0f);
				else
					pos[1] = HUD_CROSSHAIR_SIZE + 2.0f;
				break;
			case INDICATOR_TOP:
				pos[0] = -(labelSize[0]/2.0f);
				pos[1] = HUD_CROSSHAIR_SIZE + 2.0f;
				break;
			case INDICATOR_LEFT:
				pos[0] = HUD_CROSSHAIR_SIZE + 2.0f;
				pos[1] = -(labelSize[1]/2.0f);
				break;
			case INDICATOR_RIGHT:
				pos[0] = -(labelSize[0] + HUD_CROSSHAIR_SIZE + 2.0f);
				pos[1] = -(labelSize[1]/2.0f);
				break;
			case INDICATOR_BOTTOM:
				pos[0] = -(labelSize[0]/2.0f);
				pos[1] = -(labelSize[1] + HUD_CROSSHAIR_SIZE + 2.0f);
				break;
			}

			pos[0] = Clamp(pos[0] + indicator.pos.x, BORDER, w - BORDER - labelSize[0]);
			pos[1] = Clamp(pos[1] + indicator.pos.y, BORDER, h - BORDER_BOTTOM - labelSize[1]);
			MoveChild(indicator.label, pos[0], pos[1]);
			indicator.label->Show();
		} else {
			indicator.label->Hide();
		}
	}
}

void WorldView::HideIndicator(Indicator &indicator)
{
	indicator.side = INDICATOR_HIDDEN;
	indicator.pos = vector2f(0.0f, 0.0f);
	if (indicator.label)
		indicator.label->Hide();
}

void WorldView::SeparateLabels(Gui::Label *a, Gui::Label *b)
{
	float posa[2], posb[2], sizea[2], sizeb[2];
	GetChildPosition(a, posa);
	a->GetSize(sizea);
	sizea[0] *= 0.5f;
	sizea[1] *= 0.5f;
	posa[0] += sizea[0];
	posa[1] += sizea[1];
	GetChildPosition(b, posb);
	b->GetSize(sizeb);
	sizeb[0] *= 0.5f;
	sizeb[1] *= 0.5f;
	posb[0] += sizeb[0];
	posb[1] += sizeb[1];

	float overlapX = sizea[0] + sizeb[0] - fabs(posa[0] - posb[0]);
	float overlapY = sizea[1] + sizeb[1] - fabs(posa[1] - posb[1]);

	if (overlapX > 0.0f && overlapY > 0.0f) {
		if (overlapX <= 4.0f) {
			// small horizontal overlap; bump horizontally
			if (posa[0] > posb[0]) overlapX *= -1.0f;
			MoveChild(a, posa[0] - overlapX*0.5f - sizea[0], posa[1] - sizea[1]);
			MoveChild(b, posb[0] + overlapX*0.5f - sizeb[0], posb[1] - sizeb[1]);
		} else {
			// large horizonal overlap; bump vertically
			if (posa[1] > posb[1]) overlapY *= -1.0f;
			MoveChild(a, posa[0] - sizea[0], posa[1] - overlapY*0.5f - sizea[1]);
			MoveChild(b, posb[0] - sizeb[0], posb[1] + overlapY*0.5f - sizeb[1]);
		}
	}
}

double getSquareDistance(double initialDist, double scalingFactor, int num) {
	return pow(scalingFactor, num - 1) * num * initialDist;
}

double getSquareHeight(double distance, double angle) {
	return distance * tan(angle);
}

void WorldView::Draw()
{
	assert(Pi::game);
	assert(Pi::player);
	assert(!Pi::player->IsDead());

	m_renderer->ClearDepthBuffer();

	View::Draw();

	// don't draw crosshairs etc in hyperspace
	if (Pi::player->GetFlightState() == Ship::HYPERSPACE) return;

	//glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT);
	glLineWidth(2.0f);

	Color white(255, 255, 255, 204);
	Color green(0, 255, 0, 204);
	Color yellow(230, 230, 77, 255);
	Color red(255, 0, 0, 128);

	// nav target square
	DrawTargetSquare(m_navTargetIndicator, green);

	glLineWidth(1.0f);

	// velocity indicators
	DrawVelocityIndicator(m_velIndicator, white);
	DrawVelocityIndicator(m_navVelIndicator, green);

	glLineWidth(2.0f);

	DrawImageIndicator(m_mouseDirIndicator, m_indicatorMousedir.get(), yellow);

	glLineWidth(1.0f);

	// normal crosshairs
	if (GetCamType() == CamType::CAM_INTERNAL) {
		switch (m_internalCameraController->GetMode()) {
			case InternalCameraController::MODE_FRONT:
				DrawCrosshair(Gui::Screen::GetWidth()/2.0f, Gui::Screen::GetHeight()/2.0f, HUD_CROSSHAIR_SIZE, white);
				break;
			case InternalCameraController::MODE_REAR:
				DrawCrosshair(Gui::Screen::GetWidth()/2.0f, Gui::Screen::GetHeight()/2.0f, HUD_CROSSHAIR_SIZE/2.0f, white);
				break;
			default:
				break;
		}
	}

	//glPopAttrib();
}

void WorldView::DrawCrosshair(float px, float py, float sz, const Color &c)
{
	const vector2f vts[] = {
		vector2f(px-sz, py),
		vector2f(px-0.5f*sz, py),
		vector2f(px+sz, py),
		vector2f(px+0.5f*sz, py),
		vector2f(px, py-sz),
		vector2f(px, py-0.5f*sz),
		vector2f(px, py+sz),
		vector2f(px, py+0.5f*sz)
	};
	m_renderer->DrawLines2D(COUNTOF(vts), vts, c, m_blendState);
}

void WorldView::DrawTargetSquare(const Indicator &marker, const Color &c)
{
	if (marker.side == INDICATOR_HIDDEN) return;
	if (marker.side != INDICATOR_ONSCREEN)
		DrawEdgeMarker(marker, c);

	// if the square is off-screen, draw a little square at the edge
	const float sz = (marker.side == INDICATOR_ONSCREEN)
		? float(WorldView::PICK_OBJECT_RECT_SIZE * 0.5) : 3.0f;

	const float x1 = float(marker.pos.x - sz);
	const float x2 = float(marker.pos.x + sz);
	const float y1 = float(marker.pos.y - sz);
	const float y2 = float(marker.pos.y + sz);

	const vector2f vts[] = {
		vector2f(x1, y1),
		vector2f(x2, y1),
		vector2f(x2, y2),
		vector2f(x1, y2)
	};
	m_renderer->DrawLines2D(COUNTOF(vts), vts, c, m_blendState, Graphics::LINE_LOOP);
}

void WorldView::DrawVelocityIndicator(const Indicator &marker, const Color &c)
{
	if (marker.side == INDICATOR_HIDDEN) return;

	const float sz = HUD_CROSSHAIR_SIZE;
	if (marker.side == INDICATOR_ONSCREEN) {
		const float posx = marker.pos.x;
		const float posy = marker.pos.y;
		const vector2f vts[] = {
			vector2f(posx-sz, posy-sz),
			vector2f(posx-0.5f*sz, posy-0.5f*sz),
			vector2f(posx+sz, posy-sz),
			vector2f(posx+0.5f*sz, posy-0.5f*sz),
			vector2f(posx+sz, posy+sz),
			vector2f(posx+0.5f*sz, posy+0.5f*sz),
			vector2f(posx-sz, posy+sz),
			vector2f(posx-0.5f*sz, posy+0.5f*sz)
		};
		m_renderer->DrawLines2D(COUNTOF(vts), vts, c, m_blendState);
	} else
		DrawEdgeMarker(marker, c);

}

void WorldView::DrawImageIndicator(const Indicator &marker, Gui::TexturedQuad *quad, const Color &c)
{
	if (marker.side == INDICATOR_HIDDEN) return;

	if (marker.side == INDICATOR_ONSCREEN) {
		vector2f pos = marker.pos - m_indicatorMousedirSize/2.0f;
		quad->Draw(Pi::renderer, pos, m_indicatorMousedirSize, c);
	} else
		DrawEdgeMarker(marker, c);
}

void WorldView::DrawEdgeMarker(const Indicator &marker, const Color &c)
{
	const float sz = HUD_CROSSHAIR_SIZE;

	const vector2f screenCentre(Gui::Screen::GetWidth()/2.0f, Gui::Screen::GetHeight()/2.0f);
	vector2f dir = screenCentre - marker.pos;
	float len = dir.Length();
	dir *= sz/len;
	const vector2f vts[] = { marker.pos, marker.pos + dir };
	m_renderer->DrawLines2D(2, vts, c, m_blendState);
}

void WorldView::MouseWheel(bool up)
{
	if (this == Pi::GetView())
	{
		if (m_activeCameraController->IsExternal()) {
			MoveableCameraController *cam = static_cast<MoveableCameraController*>(m_activeCameraController);

			if (!up)	// Zoom out
				cam->ZoomEvent( ZOOM_SPEED * WHEEL_SENSITIVITY);
			else
				cam->ZoomEvent(-ZOOM_SPEED * WHEEL_SENSITIVITY);
		}
	}
}
NavTunnelWidget::NavTunnelWidget(WorldView *worldview, Graphics::RenderState *rs)
	: Widget()
	, m_worldView(worldview)
	, m_renderState(rs)
{
}

void NavTunnelWidget::Draw() {
	if (!Pi::IsNavTunnelDisplayed()) return;

	Body *navtarget = Pi::player->GetNavTarget();
	if (navtarget) {
		const vector3d navpos = navtarget->GetPositionRelTo(Pi::player);
		const matrix3x3d &rotmat = Pi::player->GetOrient();
		const vector3d eyevec = rotmat * m_worldView->m_activeCameraController->GetOrient().VectorZ();
		if (eyevec.Dot(navpos) >= 0.0) return;

		const Color green = Color(0, 255, 0, 204);

		const double distToDest = Pi::player->GetPositionRelTo(navtarget).Length();

		const int maxSquareHeight = std::max(Gui::Screen::GetWidth(), Gui::Screen::GetHeight()) / 2;
		const double angle = atan(maxSquareHeight / distToDest);
		const vector2f tpos(m_worldView->m_navTargetIndicator.realpos);
		const vector2f distDiff(tpos - vector2f(Gui::Screen::GetWidth() / 2.0f, Gui::Screen::GetHeight() / 2.0f));

		double dist = 0.0;
		const double scalingFactor = 1.6; // scales distance between squares: closer to 1.0, more squares
		for (int squareNum = 1; ; squareNum++) {
			dist = getSquareDistance(10.0, scalingFactor, squareNum);
			if (dist > distToDest)
				break;

			const double sqh = getSquareHeight(dist, angle);
			if (sqh >= 10) {
				const vector2f off = distDiff * (dist / distToDest);
				const vector2f sqpos(tpos-off);
				DrawTargetGuideSquare(sqpos, sqh, green);
			}
		}
	}
}

void NavTunnelWidget::DrawTargetGuideSquare(const vector2f &pos, const float size, const Color &c)
{
	const float x1 = pos.x - size;
	const float x2 = pos.x + size;
	const float y1 = pos.y - size;
	const float y2 = pos.y + size;

	const vector3f vts[] = {
		vector3f(x1,    y1,    0.f),
		vector3f(pos.x, y1,    0.f),
		vector3f(x2,    y1,    0.f),
		vector3f(x2,    pos.y, 0.f),
		vector3f(x2,    y2,    0.f),
		vector3f(pos.x, y2,    0.f),
		vector3f(x1,    y2,    0.f),
		vector3f(x1,    pos.y, 0.f)
	};
	Color black(c);
	black.a = c.a / 6;
	const Color col[] = {
		c,
		black,
		c,
		black,
		c,
		black,
		c,
		black
	};
	assert(COUNTOF(col) == COUNTOF(vts));
	m_worldView->m_renderer->DrawLines(COUNTOF(vts), vts, col, m_renderState, Graphics::LINE_LOOP);
}

void NavTunnelWidget::GetSizeRequested(float size[2]) {
	size[0] = Gui::Screen::GetWidth();
	size[1] = Gui::Screen::GetHeight();
}
