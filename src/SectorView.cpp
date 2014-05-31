// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "libs.h"
#include "GalacticView.h"
#include "Game.h"
#include "Lang.h"
#include "Pi.h"
#include "Player.h"
#include "SectorView.h"
#include "Serializer.h"
#include "StringF.h"
#include "SystemInfoView.h"
#include "galaxy/Sector.h"
#include "galaxy/GalaxyCache.h"
#include "galaxy/StarSystem.h"
#include "graphics/Graphics.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "gui/Gui.h"
#include "KeyBindings.h"
#include <algorithm>
#include <sstream>
#include <SDL_stdinc.h>

using namespace Graphics;

static const int DRAW_RAD = 5;
#define INNER_RADIUS (Sector::SIZE*1.5f)
#define OUTER_RADIUS (Sector::SIZE*float(DRAW_RAD))
static const float FAR_LIMIT = 7.5f;

enum DetailSelection {
	DETAILBOX_NONE    = 0
,	DETAILBOX_INFO    = 1
};

static const float ZOOM_SPEED = 15;
static const float WHEEL_SENSITIVITY = .03f;		// Should be a variable in user settings.

SectorView::SectorView() : UIView()
{
	InitDefaults();

	m_rotX = m_rotXMovingTo = m_rotXDefault;
	m_rotZ = m_rotZMovingTo = m_rotZDefault;
	m_zoom = m_zoomMovingTo = m_zoomDefault;

	// XXX I have no idea if this is correct,
	// I just copied it from the one other place m_zoomClamped is set
	m_zoomClamped = Clamp(m_zoom, 1.f, FAR_LIMIT);

	m_inSystem = true;

	m_current = Pi::game->GetSpace()->GetStarSystem()->GetSystemPath();
	assert(!m_current.IsSectorPath());
	m_current = m_current.SystemOnly();

	m_selected = m_hyperspaceTarget = m_current;

	GotoSystem(m_current);
	m_pos = m_posMovingTo;

	m_matchTargetToSelection   = true;
	m_automaticSystemSelection = true;
	m_detailBoxVisible         = DETAILBOX_INFO;

	InitObject();
}

SectorView::SectorView(Serializer::Reader &rd) : UIView()
{
	InitDefaults();

	m_pos.x = m_posMovingTo.x = rd.Float();
	m_pos.y = m_posMovingTo.y = rd.Float();
	m_pos.z = m_posMovingTo.z = rd.Float();
	m_rotX = m_rotXMovingTo = rd.Float();
	m_rotZ = m_rotZMovingTo = rd.Float();
	m_zoom = m_zoomMovingTo = rd.Float();
	// XXX I have no idea if this is correct,
	// I just copied it from the one other place m_zoomClamped is set
	m_zoomClamped = Clamp(m_zoom, 1.f, FAR_LIMIT);
	m_inSystem = rd.Bool();
	m_current = SystemPath::Unserialize(rd);
	m_selected = SystemPath::Unserialize(rd);
	m_hyperspaceTarget = SystemPath::Unserialize(rd);
	m_matchTargetToSelection = rd.Bool();
	m_automaticSystemSelection = rd.Bool();
	m_detailBoxVisible = rd.Byte();

	InitObject();
}

void SectorView::InitDefaults()
{
	m_rotXDefault = Pi::config->Float("SectorViewXRotation");
	m_rotZDefault = Pi::config->Float("SectorViewZRotation");
	m_zoomDefault = Pi::config->Float("SectorViewZoom");
	m_rotXDefault = Clamp(m_rotXDefault, -170.0f, -10.0f);
	m_zoomDefault = Clamp(m_zoomDefault, 0.1f, 5.0f);
	m_previousSearch = "";

	m_secPosFar = vector3f(INT_MAX, INT_MAX, INT_MAX);
	m_radiusFar = 0;
	m_cacheXMin = 0;
	m_cacheXMax = 0;
	m_cacheYMin = 0;
	m_cacheYMax = 0;
	m_cacheYMin = 0;
	m_cacheYMax = 0;

	m_sectorCache = Sector::cache.NewSlaveCache();
}

void SectorView::InitObject()
{
	SetTransparency(true);

	m_lineVerts.reset(new Graphics::VertexArray(Graphics::ATTRIB_POSITION, 500));
	m_secLineVerts.reset(new Graphics::VertexArray(Graphics::ATTRIB_POSITION, 500));

	Gui::Screen::PushFont("OverlayFont");
	m_clickableLabels = new Gui::LabelSet();
	m_clickableLabels->SetLabelColor(Color(178,178,178,191));
	Add(m_clickableLabels, 0, 0);
	Gui::Screen::PopFont();

	m_sectorLabel = new Gui::Label("");
	Add(m_sectorLabel, 2, Gui::Screen::GetHeight()-Gui::Screen::GetFontHeight()*2-66);
	m_distanceLabel = new Gui::Label("");
	Add(m_distanceLabel, 2, Gui::Screen::GetHeight()-Gui::Screen::GetFontHeight()-66);

	m_zoomInButton = new Gui::ImageButton("icons/zoom_in.png");
	m_zoomInButton->SetToolTip(Lang::ZOOM_IN);
	m_zoomInButton->SetRenderDimensions(30, 22);
	Add(m_zoomInButton, 700, 5);

	m_zoomLevelLabel = (new Gui::Label(""))->Color(69, 219, 235);
	Add(m_zoomLevelLabel, 640, 5);

	m_zoomOutButton = new Gui::ImageButton("icons/zoom_out.png");
	m_zoomOutButton->SetToolTip(Lang::ZOOM_OUT);
	m_zoomOutButton->SetRenderDimensions(30, 22);
	Add(m_zoomOutButton, 732, 5);

	Gui::Screen::PushFont("OverlayFont");

	Add(new Gui::Label(Lang::SEARCH), 650, 470);
	m_searchBox = new Gui::TextEntry();
	m_searchBox->onKeyPress.connect(sigc::mem_fun(this, &SectorView::OnSearchBoxKeyPress));
	Add(m_searchBox, 700, 470);

	m_statusLabel = new Gui::Label("");
	Add(m_statusLabel, 650, 490);
	Gui::Screen::PopFont();

	Gui::Screen::PushFont("OverlayFont");

	m_renderer = Pi::renderer; //XXX pass cleanly to all views constructors!

	Graphics::RenderStateDesc rsd;
	m_solidState = m_renderer->CreateRenderState(rsd);

	rsd.blendMode = Graphics::BLEND_ALPHA;
	rsd.depthWrite = false;
	m_alphaBlendState = m_renderer->CreateRenderState(rsd);

	Graphics::MaterialDescriptor bbMatDesc;
	bbMatDesc.effect = Graphics::EFFECT_SPHEREIMPOSTOR;
	m_starMaterial = m_renderer->CreateMaterial(bbMatDesc);

	m_disk.reset(new Graphics::Drawables::Disk(m_renderer, m_solidState, Color::WHITE, 0.2f));

	m_infoBox = new Gui::VBox();
	m_infoBox->SetTransparency(false);
	m_infoBox->SetBgColor(Color(16,16,32,128));
	m_infoBox->SetSpacing(10.0f);
	Add(m_infoBox, 5, 5);

	// 1. holds info about current, targeted, selected systems
	Gui::VBox *locationsBox = new Gui::VBox();
	locationsBox->SetSpacing(5.f);
	// 1.1 current system
	Gui::VBox *systemBox = new Gui::VBox();
	Gui::HBox *hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	Gui::Button *b = new Gui::SolidButton();
	b->onClick.connect(sigc::mem_fun(this, &SectorView::GotoCurrentSystem));
	hbox->PackEnd(b);
	hbox->PackEnd((new Gui::Label(Lang::CURRENT_SYSTEM))->Color(255, 255, 255));
	systemBox->PackEnd(hbox);
	hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	m_currentSystemLabels.systemName = (new Gui::Label(""))->Color(255, 255, 0);
	m_currentSystemLabels.sector = (new Gui::Label(""))->Color(255, 255, 0);
	m_currentSystemLabels.distance.label = (new Gui::Label(""))->Color(255, 0, 0);
	m_currentSystemLabels.distance.line = NULL;
	m_currentSystemLabels.distance.okayColor = ::Color(0, 255, 0);
	hbox->PackEnd(m_currentSystemLabels.systemName);
	hbox->PackEnd(m_currentSystemLabels.sector);
	systemBox->PackEnd(hbox);
	systemBox->PackEnd(m_currentSystemLabels.distance.label);
	m_currentSystemLabels.starType = (new Gui::Label(""))->Color(255, 0, 255);
	systemBox->PackEnd(m_currentSystemLabels.starType);
	locationsBox->PackEnd(systemBox);
	// 1.2 targeted system
	systemBox = new Gui::VBox();
	hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	b = new Gui::SolidButton();
	b->onClick.connect(sigc::mem_fun(this, &SectorView::GotoHyperspaceTarget));
	hbox->PackEnd(b);
	hbox->PackEnd((new Gui::Label(Lang::HYPERSPACE_TARGET))->Color(255, 255, 255));
    m_hyperspaceLockLabel = (new Gui::Label(""))->Color(255, 255, 255);
    hbox->PackEnd(m_hyperspaceLockLabel);
	systemBox->PackEnd(hbox);
	hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	m_targetSystemLabels.systemName = (new Gui::Label(""))->Color(255, 255, 0);
	m_targetSystemLabels.sector = (new Gui::Label(""))->Color(255, 255, 0);
	m_targetSystemLabels.distance.label = (new Gui::Label(""))->Color(255, 0, 0);
	m_targetSystemLabels.distance.line = &m_jumpLine;
	m_targetSystemLabels.distance.okayColor = ::Color(0, 255, 0);
	hbox->PackEnd(m_targetSystemLabels.systemName);
	hbox->PackEnd(m_targetSystemLabels.sector);
	systemBox->PackEnd(hbox);
	systemBox->PackEnd(m_targetSystemLabels.distance.label);
	m_targetSystemLabels.starType = (new Gui::Label(""))->Color(255, 0, 255);
	systemBox->PackEnd(m_targetSystemLabels.starType);
	m_secondDistance.label = (new Gui::Label(""))->Color(0, 128, 255);
	m_secondDistance.line = &m_secondLine;
	m_secondDistance.okayColor = ::Color(51, 153, 128);
	systemBox->PackEnd(m_secondDistance.label);
	locationsBox->PackEnd(systemBox);
	// 1.3 selected system
	systemBox = new Gui::VBox();
	hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	b = new Gui::SolidButton();
	b->onClick.connect(sigc::mem_fun(this, &SectorView::GotoSelectedSystem));
	hbox->PackEnd(b);
	hbox->PackEnd((new Gui::Label(Lang::SELECTED_SYSTEM))->Color(255, 255, 255));
	systemBox->PackEnd(hbox);
	hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	m_selectedSystemLabels.systemName = (new Gui::Label(""))->Color(255, 255, 0);
	m_selectedSystemLabels.sector = (new Gui::Label(""))->Color(255, 255, 0);
	m_selectedSystemLabels.distance.label = (new Gui::Label(""))->Color(255, 0, 0);
	m_selectedSystemLabels.distance.line = &m_selectedLine;
	m_selectedSystemLabels.distance.okayColor = ::Color(0, 255, 0);
	hbox->PackEnd(m_selectedSystemLabels.systemName);
	hbox->PackEnd(m_selectedSystemLabels.sector);
	systemBox->PackEnd(hbox);
	systemBox->PackEnd(m_selectedSystemLabels.distance.label);
	m_selectedSystemLabels.starType = (new Gui::Label(""))->Color(255, 0, 255);
	systemBox->PackEnd(m_selectedSystemLabels.starType);
	locationsBox->PackEnd(systemBox);
	m_infoBox->PackEnd(locationsBox);

	// 2. holds options for displaying systems
	Gui::VBox *filterBox = new Gui::VBox();
	// 2.1 Draw vertical lines
	hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	m_drawSystemLegButton = (new Gui::ToggleButton());
	m_drawSystemLegButton->SetPressed(false); // TODO: replace with var
	hbox->PackEnd(m_drawSystemLegButton);
	Gui::Label *label = (new Gui::Label(Lang::DRAW_VERTICAL_LINES))->Color(255, 255, 255);
	hbox->PackEnd(label);
	filterBox->PackEnd(hbox);
	// 2.1 Selection follows movement
	hbox = new Gui::HBox();
	hbox->SetSpacing(5.0f);
	m_automaticSystemSelectionButton = (new Gui::ToggleButton());
	m_automaticSystemSelectionButton->SetPressed(m_automaticSystemSelection);
    m_automaticSystemSelectionButton->onChange.connect(sigc::mem_fun(this, &SectorView::OnAutomaticSystemSelectionChange));
	hbox->PackEnd(m_automaticSystemSelectionButton);
	label = (new Gui::Label(Lang::AUTOMATIC_SYSTEM_SELECTION))->Color(255, 255, 255);
	hbox->PackEnd(label);
	filterBox->PackEnd(hbox);

	m_infoBox->PackEnd(filterBox);

	m_onMouseWheelCon =
		Pi::onMouseWheel.connect(sigc::mem_fun(this, &SectorView::MouseWheel));

	UpdateSystemLabels(m_currentSystemLabels, m_current);
	UpdateSystemLabels(m_targetSystemLabels, m_hyperspaceTarget);
	UpdateSystemLabels(m_selectedSystemLabels, m_selected);
	UpdateDistanceLabelAndLine(m_secondDistance, m_selected, m_hyperspaceTarget);
	UpdateHyperspaceLockLabel();
}

SectorView::~SectorView()
{
	m_onMouseWheelCon.disconnect();
	if (m_onKeyPressConnection.connected()) m_onKeyPressConnection.disconnect();
}

void SectorView::Save(Serializer::Writer &wr)
{
	wr.Float(m_pos.x);
	wr.Float(m_pos.y);
	wr.Float(m_pos.z);
	wr.Float(m_rotX);
	wr.Float(m_rotZ);
	wr.Float(m_zoom);
	wr.Bool(m_inSystem);
	m_current.Serialize(wr);
	m_selected.Serialize(wr);
	m_hyperspaceTarget.Serialize(wr);
	wr.Bool(m_matchTargetToSelection);
	wr.Bool(m_automaticSystemSelection);
	wr.Byte(m_detailBoxVisible);
}

void SectorView::OnSearchBoxKeyPress(const SDL_Keysym *keysym)
{
	//remember the last search text, hotkey: up
	if (m_searchBox->GetText().empty() && keysym->sym == SDLK_UP && !m_previousSearch.empty())
		m_searchBox->SetText(m_previousSearch);

	if (keysym->sym != SDLK_KP_ENTER && keysym->sym != SDLK_RETURN)
		return;

	std::string search = m_searchBox->GetText();
	if (!search.size())
		return;

	m_previousSearch = search;

	//Try to detect if user entered a sector address, comma or space separated, strip parentheses
	//system index is unreliable, so it is not supported
	try {
		GotoSector(SystemPath::Parse(search.c_str()));
		return;
	} catch (SystemPath::ParseFailure) {}

	bool gotMatch = false, gotStartMatch = false;
	SystemPath bestMatch;
	const std::string *bestMatchName = 0;

	for (auto i = m_sectorCache->Begin(); i != m_sectorCache->End(); ++i)

		for (unsigned int systemIndex = 0; systemIndex < (*i).second->m_systems.size(); systemIndex++) {
			const Sector::System *ss = &((*i).second->m_systems[systemIndex]);

			// compare with the start of the current system
			if (strncasecmp(search.c_str(), ss->name.c_str(), search.size()) == 0) {

				// matched, see if they're the same size
				if (search.size() == ss->name.size()) {

					// exact match, take it and go
					SystemPath path = (*i).first;
					path.systemIndex = systemIndex;
					m_statusLabel->SetText(stringf(Lang::EXACT_MATCH_X, formatarg("system", ss->name)));
					GotoSystem(path);
					return;
				}

				// partial match at start of name
				if (!gotMatch || !gotStartMatch || bestMatchName->size() > ss->name.size()) {

					// don't already have one or its shorter than the previous
					// one, take it
					bestMatch = (*i).first;
					bestMatch.systemIndex = systemIndex;
					bestMatchName = &(ss->name);
					gotMatch = gotStartMatch = true;
				}

				continue;
			}

			// look for the search term somewhere within the current system
			if (pi_strcasestr(ss->name.c_str(), search.c_str())) {

				// found it
				if (!gotMatch || !gotStartMatch || bestMatchName->size() > ss->name.size()) {

					// best we've found so far, take it
					bestMatch = (*i).first;
					bestMatch.systemIndex = systemIndex;
					bestMatchName = &(ss->name);
					gotMatch = true;
				}
			}
		}

	if (gotMatch) {
		m_statusLabel->SetText(stringf(Lang::NOT_FOUND_BEST_MATCH_X, formatarg("system", *bestMatchName)));
		GotoSystem(bestMatch);
	}

	else
		m_statusLabel->SetText(Lang::NOT_FOUND);
}

#define FFRAC(_x)	((_x)-floor(_x))

void SectorView::Draw3D()
{
	PROFILE_SCOPED()

	m_lineVerts->Clear();
	m_secLineVerts->Clear();
	m_clickableLabels->Clear();

	m_renderer->SetPerspectiveProjection(40.f, m_renderer->GetDisplayAspect(), 1.f, 300.f);

	matrix4x4f modelview = matrix4x4f::Identity();

	m_renderer->ClearScreen();

	m_sectorLabel->SetText(stringf(Lang::SECTOR_X_Y_Z,
		formatarg("x", int(floorf(m_pos.x))),
		formatarg("y", int(floorf(m_pos.y))),
		formatarg("z", int(floorf(m_pos.z)))));

	m_zoomLevelLabel->SetText(stringf(Lang::NUMBER_LY, formatarg("distance", ((m_zoomClamped/FAR_LIMIT )*(OUTER_RADIUS)) + 0.5 * Sector::SIZE)));

	if (m_inSystem) {
		vector3f dv = vector3f(floorf(m_pos.x)-m_current.sectorX, floorf(m_pos.y)-m_current.sectorY, floorf(m_pos.z)-m_current.sectorZ) * Sector::SIZE;
		m_distanceLabel->SetText(stringf(Lang::DISTANCE_LY, formatarg("distance", dv.Length())));
	}
	else {
		m_distanceLabel->SetText("");
	}

	Graphics::Renderer::MatrixTicket ticket(m_renderer, Graphics::MatrixMode::MODELVIEW);

	// units are lightyears, my friend
	modelview.Translate(0.f, 0.f, -10.f-10.f*m_zoom);    // not zoomClamped, let us zoom out a bit beyond what we're drawing
	modelview.Rotate(DEG2RAD(m_rotX), 1.f, 0.f, 0.f);
	modelview.Rotate(DEG2RAD(m_rotZ), 0.f, 0.f, 1.f);
	modelview.Translate(-FFRAC(m_pos.x)*Sector::SIZE, -FFRAC(m_pos.y)*Sector::SIZE, -FFRAC(m_pos.z)*Sector::SIZE);
	m_renderer->SetTransform(modelview);

	DrawNearSectors(modelview);

	m_renderer->SetTransform(matrix4x4f::Identity());

	//draw star billboards in one go
	m_renderer->SetAmbientColor(Color(30));
	if( m_starBuffer.Valid() )
		m_renderer->DrawBuffer(m_starBuffer.Get(), m_solidState, m_starMaterial);

	//draw sector legs in one go
	if (m_lineVerts->GetNumVerts() > 2)
		m_renderer->DrawLines(m_lineVerts->GetNumVerts(), &m_lineVerts->position[0], &m_lineVerts->diffuse[0], m_alphaBlendState);

	if (m_secLineVerts->GetNumVerts() > 2)
		m_renderer->DrawLines(m_secLineVerts->GetNumVerts(), &m_secLineVerts->position[0], &m_secLineVerts->diffuse[0], m_alphaBlendState);

	UIView::Draw3D();
}

void SectorView::SetHyperspaceTarget(const SystemPath &path)
{
	m_hyperspaceTarget = path;
	m_matchTargetToSelection = false;
	onHyperspaceTargetChanged.emit();

	UpdateDistanceLabelAndLine(m_secondDistance, m_selected, m_hyperspaceTarget);
	UpdateHyperspaceLockLabel();
	UpdateSystemLabels(m_targetSystemLabels, m_hyperspaceTarget);
}

void SectorView::FloatHyperspaceTarget()
{
	m_matchTargetToSelection = true;
	UpdateHyperspaceLockLabel();
}

void SectorView::UpdateHyperspaceLockLabel()
{
	m_hyperspaceLockLabel->SetText(stringf("[%0]", m_matchTargetToSelection ? std::string(Lang::FOLLOWING_SELECTION) : std::string(Lang::LOCKED)));
}

void SectorView::ResetHyperspaceTarget()
{
	SystemPath old = m_hyperspaceTarget;
	m_hyperspaceTarget = m_selected;
	FloatHyperspaceTarget();

	if (!old.IsSameSystem(m_hyperspaceTarget)) {
		onHyperspaceTargetChanged.emit();
		UpdateDistanceLabelAndLine(m_secondDistance, m_selected, m_hyperspaceTarget);
		UpdateSystemLabels(m_targetSystemLabels, m_hyperspaceTarget);
	} else {
		if (m_detailBoxVisible == DETAILBOX_INFO) m_infoBox->ShowAll();
	}
}

void SectorView::GotoSector(const SystemPath &path)
{
	m_posMovingTo = vector3f(path.sectorX, path.sectorY, path.sectorZ);
}

void SectorView::GotoSystem(const SystemPath &path)
{
	RefCountedPtr<Sector> ps = GetCached(path);
	const vector3f &p = ps->m_systems[path.systemIndex].p;
	m_posMovingTo.x = path.sectorX + p.x/Sector::SIZE;
	m_posMovingTo.y = path.sectorY + p.y/Sector::SIZE;
	m_posMovingTo.z = path.sectorZ + p.z/Sector::SIZE;
}

void SectorView::SetSelected(const SystemPath &path)
{
    m_selected = path;

	if (m_matchTargetToSelection && m_selected != m_current) {
		m_hyperspaceTarget = m_selected;
		onHyperspaceTargetChanged.emit();
		UpdateSystemLabels(m_targetSystemLabels, m_hyperspaceTarget);
	}

	UpdateDistanceLabelAndLine(m_secondDistance, m_selected, m_hyperspaceTarget);
	UpdateSystemLabels(m_selectedSystemLabels, m_selected);
}

void SectorView::OnClickSystem(const SystemPath &path)
{
	if (path.IsSameSystem(m_selected)) {
		RefCountedPtr<StarSystem> system = StarSystem::cache->GetCached(path);
		if (system->GetNumStars() > 1 && m_selected.IsBodyPath()) {
			int i;
			for (i = 0; i < system->GetNumStars(); ++i)
				if (system->GetStars()[i]->GetPath() == m_selected) break;
			if (i >= system->GetNumStars() - 1)
				SetSelected(system->GetStars()[0]->GetPath());
			else
				SetSelected(system->GetStars()[i+1]->GetPath());
		} else {
			SetSelected(system->GetStars()[0]->GetPath());
		}
	} else {
		if (m_automaticSystemSelection) {
			GotoSystem(path);
		} else {
			RefCountedPtr<StarSystem> system = StarSystem::cache->GetCached(path);
			SetSelected(system->GetStars()[0]->GetPath());
		}
	}
}

void SectorView::PutSystemLabels(RefCountedPtr<Sector> sec, const vector3f &origin, int drawRadius)
{
	PROFILE_SCOPED()
	Uint32 sysIdx = 0;
	for (std::vector<Sector::System>::iterator sys = sec->m_systems.begin(); sys !=sec->m_systems.end(); ++sys, ++sysIdx) {
		// skip the system if it doesn't fall within the sphere we're viewing.
		if ((m_pos*Sector::SIZE - (*sys).FullPosition()).Length() > drawRadius) continue;

		// place the label
		vector3d systemPos = vector3d((*sys).FullPosition() - origin);
		vector3d screenPos;
		if (Gui::Screen::Project(systemPos, screenPos)) {
			// reject back-projected labels
			if(screenPos.z > 1.0f)
				continue;

			// get a system path to pass to the event handler when the label is licked
			SystemPath sysPath = SystemPath((*sys).sx, (*sys).sy, (*sys).sz, sysIdx);

			// setup the label;
			m_clickableLabels->Add((*sys).name, sigc::bind(sigc::mem_fun(this, &SectorView::OnClickSystem), sysPath), screenPos.x, screenPos.y, Color::WHITE);
		}
	}
}

void SectorView::AddStarBillboard(Graphics::VertexArray &va, const matrix4x4f &trans, const vector3f &pos, const Color &col, float size)
{
	const matrix3x3f rot = trans.GetOrient().Transpose();

	const vector3f offset = trans * pos;

	const vector3f rotv1 = rot * vector3f(size/2.f, -size/2.f, 0.0f);
	const vector3f rotv2 = rot * vector3f(size/2.f, size/2.f, 0.0f);

	va.Add(offset-rotv1, col, vector2f(0.f, 0.f)); //top left
	va.Add(offset-rotv2, col, vector2f(0.f, 1.f)); //bottom left
	va.Add(offset+rotv2, col, vector2f(1.f, 0.f)); //top right

	va.Add(offset+rotv2, col, vector2f(1.f, 0.f)); //top right
	va.Add(offset-rotv2, col, vector2f(0.f, 1.f)); //bottom left
	va.Add(offset+rotv1, col, vector2f(1.f, 1.f)); //bottom right
}

void SectorView::UpdateDistanceLabelAndLine(DistanceIndicator &distance, const SystemPath &src, const SystemPath &dest)
{
	PROFILE_SCOPED()

	if (src.IsSameSystem(dest)) {
		distance.label->SetText("");
	} else {
		RefCountedPtr<const Sector> sec = GetCached(dest);
		RefCountedPtr<const Sector> srcSec = GetCached(src);

		char format[256];

		const float dist = Sector::DistanceBetween(sec, dest.systemIndex, srcSec, src.systemIndex);

		double dur;
		enum Ship::HyperjumpStatus jumpStatus = Pi::player->GetHyperspaceDetails(src, dest, dur);
		const double DaysNeeded = dur*(1.0 / (24*60*60));
		const double HoursNeeded = (DaysNeeded - floor(DaysNeeded))*24;

		switch (jumpStatus) {
			case Ship::HYPERJUMP_OK:
				snprintf(format, sizeof(format), "[ %s | %s, %s ]", Lang::NUMBER_LY, Lang::NUMBER_DAYS, Lang::NUMBER_HOURS);
				distance.label->SetText(stringf(format,
					formatarg("distance", dist), formatarg("days", floor(DaysNeeded)), formatarg("hours", HoursNeeded)));
				distance.label->Color(distance.okayColor);
				if (distance.line)
					distance.line->SetColor(distance.okayColor);
				break;
			default:
				distance.label->SetText("");
				break;
		}
	}
}

void SectorView::UpdateSystemLabels(SystemLabels &labels, const SystemPath &path)
{
	UpdateDistanceLabelAndLine(labels.distance, m_current, path);

	RefCountedPtr<StarSystem> sys = StarSystem::cache->GetCached(path);

	std::string desc;
	if (sys->GetNumStars() == 4) {
		desc = Lang::QUADRUPLE_SYSTEM;
	} else if (sys->GetNumStars() == 3) {
		desc = Lang::TRIPLE_SYSTEM;
	} else if (sys->GetNumStars() == 2) {
		desc = Lang::BINARY_SYSTEM;
	} else {
		desc = sys->GetRootBody()->GetAstroDescription();
	}
	labels.starType->SetText(desc);

	if (path.IsBodyPath()) {
		labels.systemName->SetText(sys->GetBodyByPath(path)->GetName());
	} else {
		labels.systemName->SetText(sys->GetName());
	}
	labels.sector->SetText(stringf("(%x,%y,%z)",
		formatarg("x", int(path.sectorX)),
		formatarg("y", int(path.sectorY)),
		formatarg("z", int(path.sectorZ))));

	if (m_detailBoxVisible == DETAILBOX_INFO) m_infoBox->ShowAll();
}

void SectorView::OnAutomaticSystemSelectionChange(Gui::ToggleButton *b, bool pressed) {
    m_automaticSystemSelection = pressed;
}

void SectorView::DrawNearSectors(const matrix4x4f& modelview)
{
	PROFILE_SCOPED()

	RefCountedPtr<const Sector> playerSec = GetCached(m_current);
	const vector3f playerPos = Sector::SIZE * vector3f(float(m_current.sectorX), float(m_current.sectorY), float(m_current.sectorZ)) + playerSec->m_systems[m_current.systemIndex].p;

	for (int sx = -DRAW_RAD; sx <= DRAW_RAD; sx++) {
		for (int sy = -DRAW_RAD; sy <= DRAW_RAD; sy++) {
			for (int sz = -DRAW_RAD; sz <= DRAW_RAD; sz++) {
				DrawNearSector(int(floorf(m_pos.x))+sx, int(floorf(m_pos.y))+sy, int(floorf(m_pos.z))+sz, playerPos,
					modelview * matrix4x4f::Translation(Sector::SIZE*sx, Sector::SIZE*sy, Sector::SIZE*sz));
			}
		}
	}

	// ...then switch and do all the labels
	const vector3f secOrigin = vector3f(int(floorf(m_pos.x)), int(floorf(m_pos.y)), int(floorf(m_pos.z)));

	m_renderer->SetTransform(modelview);
	glDepthRange(0,1);
	Gui::Screen::EnterOrtho();
	for (int sx = -DRAW_RAD; sx <= DRAW_RAD; sx++) {
		for (int sy = -DRAW_RAD; sy <= DRAW_RAD; sy++) {
			for (int sz = -DRAW_RAD; sz <= DRAW_RAD; sz++) {
				PutSystemLabels(GetCached(SystemPath(sx + secOrigin.x, sy + secOrigin.y, sz + secOrigin.z)), Sector::SIZE * secOrigin, Sector::SIZE * DRAW_RAD);
			}
		}
	}
	Gui::Screen::LeaveOrtho();
}

void SectorView::DrawNearSector(const int sx, const int sy, const int sz, const vector3f &playerAbsPos,const matrix4x4f &trans)
{
	PROFILE_SCOPED()
	m_renderer->SetTransform(trans);
	RefCountedPtr<Sector> ps = GetCached(SystemPath(sx, sy, sz));

	int cz = int(floor(m_pos.z+0.5f));

	if (cz == sz) {
		const Color darkgreen(0, 51, 0, 255);
		const vector3f vts[] = {
			trans * vector3f(0.f, 0.f, 0.f),
			trans * vector3f(0.f, Sector::SIZE, 0.f),
			trans * vector3f(Sector::SIZE, Sector::SIZE, 0.f),
			trans * vector3f(Sector::SIZE, 0.f, 0.f)
		};

		m_secLineVerts->Add(vts[0], darkgreen);	// line segment 1
		m_secLineVerts->Add(vts[1], darkgreen);
		m_secLineVerts->Add(vts[1], darkgreen);	// line segment 2
		m_secLineVerts->Add(vts[2], darkgreen);
		m_secLineVerts->Add(vts[2], darkgreen);	// line segment 3
		m_secLineVerts->Add(vts[3], darkgreen);
		m_secLineVerts->Add(vts[3], darkgreen);	// line segment 4
		m_secLineVerts->Add(vts[0], darkgreen);
	}

	Graphics::VertexArray starVA(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE | Graphics::ATTRIB_UV0, 500);
	Uint32 sysIdx = 0;
	for (std::vector<Sector::System>::iterator i = ps->m_systems.begin(); i != ps->m_systems.end(); ++i, ++sysIdx) {
		// calculate where the system is in relation the centre of the view...
		const vector3f sysAbsPos = Sector::SIZE*vector3f(float(sx), float(sy), float(sz)) + (*i).p;
		const vector3f toCentreOfView = m_pos*Sector::SIZE - sysAbsPos;

		// ...and skip the system if it doesn't fall within the sphere we're viewing.
		if (toCentreOfView.Length() > OUTER_RADIUS) continue;

		const bool bIsCurrentSystem = i->IsSameSystem(m_current);

		// if the system is the current system or target we can't skip it
		bool can_skip = !i->IsSameSystem(m_selected)
						&& !i->IsSameSystem(m_hyperspaceTarget)
						&& !bIsCurrentSystem;

		// only do this once we've pretty much stopped moving.
		vector3f diff = vector3f(
				fabs(m_posMovingTo.x - m_pos.x),
				fabs(m_posMovingTo.y - m_pos.y),
				fabs(m_posMovingTo.z - m_pos.z));

		// Ideally, since this takes so f'ing long, it wants to be done as a threaded job but haven't written that yet.
		if( (diff.x < 0.001f && diff.y < 0.001f && diff.z < 0.001f) ) {
			SystemPath current = SystemPath(sx, sy, sz, sysIdx);
			RefCountedPtr<StarSystem> pSS = StarSystem::cache->GetCached(current);
		}

		matrix4x4f systrans = trans * matrix4x4f::Translation((*i).p.x, (*i).p.y, (*i).p.z);
		m_renderer->SetTransform(systrans);

		// legs
		if (m_drawSystemLegButton->GetPressed() || !can_skip) {
			const Color light(128);
			const Color dark(51);

			// draw system "leg"
			float z = -(*i).p.z;
			if (sz <= cz)
				z = z+abs(cz-sz)*Sector::SIZE;
			else
				z = z-abs(cz-sz)*Sector::SIZE;
			m_lineVerts->Add(systrans * vector3f(0.f, 0.f, z), light);
			m_lineVerts->Add(systrans * vector3f(0.f, 0.f, z * 0.5f), dark);
			m_lineVerts->Add(systrans * vector3f(0.f, 0.f, z * 0.5f), dark);
			m_lineVerts->Add(systrans * vector3f(0.f, 0.f, 0.f), light);

			//cross at other end
			m_lineVerts->Add(systrans * vector3f(-0.1f, -0.1f, z), light);
			m_lineVerts->Add(systrans * vector3f(0.1f, 0.1f, z), light);
			m_lineVerts->Add(systrans * vector3f(-0.1f, 0.1f, z), light);
			m_lineVerts->Add(systrans * vector3f(0.1f, -0.1f, z), light);
		}

		if (i->IsSameSystem(m_selected)) {
			if (m_selected != m_current) {
			    m_selectedLine.SetStart(vector3f(0.f, 0.f, 0.f));
			    m_selectedLine.SetEnd(playerAbsPos - sysAbsPos);
			    m_selectedLine.Draw(m_renderer, m_solidState);
			} else {
			    m_secondDistance.label->SetText("");
			}
			if (m_selected != m_hyperspaceTarget) {
				RefCountedPtr<Sector> hyperSec = GetCached(m_hyperspaceTarget);
				const vector3f hyperAbsPos =
					Sector::SIZE*vector3f(m_hyperspaceTarget.sectorX, m_hyperspaceTarget.sectorY, m_hyperspaceTarget.sectorZ)
					+ hyperSec->m_systems[m_hyperspaceTarget.systemIndex].p;
				if (m_selected != m_current) {
				    m_secondLine.SetStart(vector3f(0.f, 0.f, 0.f));
				    m_secondLine.SetEnd(hyperAbsPos - sysAbsPos);
				    m_secondLine.Draw(m_renderer, m_solidState);
				}

				if (m_hyperspaceTarget != m_current) {
				    // FIXME: Draw when drawing hyperjump target or current system
				    m_jumpLine.SetStart(hyperAbsPos - sysAbsPos);
				    m_jumpLine.SetEnd(playerAbsPos - sysAbsPos);
				    m_jumpLine.Draw(m_renderer, m_solidState);
				}
			} else {
			    m_secondDistance.label->SetText("");
			}
		}

		// draw star blob itself
		systrans.Rotate(DEG2RAD(-m_rotZ), 0, 0, 1);
		systrans.Rotate(DEG2RAD(-m_rotX), 1, 0, 0);
		systrans.Scale((StarSystem::starScale[(*i).starType[0]]));
		m_renderer->SetTransform(systrans);

		const Uint8 *col = StarSystem::starColors[(*i).starType[0]];
		AddStarBillboard(starVA, systrans, vector3f(0.f), Color(col[0], col[1], col[2], 255), 0.5f);

		// player location indicator
		if (m_inSystem && bIsCurrentSystem) {
			glDepthRange(0.2,1.0);
			m_disk->SetColor(Color(0, 0, 204));
			m_renderer->SetTransform(systrans * matrix4x4f::ScaleMatrix(3.f));
			m_disk->Draw(m_renderer);
		}
		// selected indicator
		if (bIsCurrentSystem) {
			glDepthRange(0.1,1.0);
			m_disk->SetColor(Color(0, 204, 0));
			m_renderer->SetTransform(systrans * matrix4x4f::ScaleMatrix(2.f));
			m_disk->Draw(m_renderer);
		}
		// hyperspace target indicator (if different from selection)
		if (i->IsSameSystem(m_hyperspaceTarget) && m_hyperspaceTarget != m_selected && (!m_inSystem || m_hyperspaceTarget != m_current)) {
			glDepthRange(0.1,1.0);
			m_disk->SetColor(Color(77));
			m_renderer->SetTransform(systrans * matrix4x4f::ScaleMatrix(2.f));
			m_disk->Draw(m_renderer);
		}
	}

	if( starVA.GetNumVerts()>0 )
	{
		//create buffer and upload data
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
		vbd.attrib[1].format   = Graphics::ATTRIB_FORMAT_UBYTE4;
		vbd.numVertices = starVA.GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;	// we could be updating this per-frame
		m_starMaterial->SetupVertexBufferDesc( vbd );
		m_starBuffer.Reset( m_renderer->CreateVertexBuffer(vbd) );
		m_starBuffer->Populate( starVA );
	} else {
		m_starBuffer.Reset();
	}
	
}

void SectorView::OnSwitchTo()
{
	m_renderer->SetViewport(0, 0, Graphics::GetScreenWidth(), Graphics::GetScreenHeight());

	if (!m_onKeyPressConnection.connected())
		m_onKeyPressConnection =
			Pi::onKeyPress.connect(sigc::mem_fun(this, &SectorView::OnKeyPressed));

	UIView::OnSwitchTo();

	Update();

	UpdateSystemLabels(m_targetSystemLabels, m_hyperspaceTarget);
	UpdateSystemLabels(m_selectedSystemLabels, m_selected);
	UpdateDistanceLabelAndLine(m_secondDistance, m_selected, m_hyperspaceTarget);
}

void SectorView::RefreshDetailBoxVisibility()
{
	if (m_detailBoxVisible != DETAILBOX_INFO)    m_infoBox->HideAll();    else m_infoBox->ShowAll();
}

void SectorView::OnKeyPressed(SDL_Keysym *keysym)
{
	if (Pi::GetView() != this) {
		m_onKeyPressConnection.disconnect();
		return;
	}

	// ignore keypresses if they're typing
	if (m_searchBox->IsFocused()) {
		// but if they press enter then we want future keys
		if (keysym->sym == SDLK_KP_ENTER || keysym->sym == SDLK_RETURN)
			m_searchBox->Unfocus();
		return;
	}

	// '/' focuses the search box
	if (KeyBindings::mapStartSearch.Matches(keysym)) {
		m_searchBox->SetText("");
		m_searchBox->GrabFocus();
		return;
	}

	// space "locks" (or unlocks) the hyperspace target to the selected system
	if (KeyBindings::mapLockHyperspaceTarget.Matches(keysym)) {
		if ((m_matchTargetToSelection || m_hyperspaceTarget != m_selected) && !m_selected.IsSameSystem(m_current))
			SetHyperspaceTarget(m_selected);
		else
			ResetHyperspaceTarget();
		return;
	}

	// cycle through the info box and nothing
	if (KeyBindings::mapToggleInfoPanel.Matches(keysym)) {
		if (m_detailBoxVisible == DETAILBOX_INFO) m_detailBoxVisible = DETAILBOX_NONE;
		else                                      m_detailBoxVisible++;
		RefreshDetailBoxVisibility();
		return;
	}

	if (KeyBindings::mapToggleSelectionFollowView.Matches(keysym)) {
		m_automaticSystemSelection = !m_automaticSystemSelection;
        m_automaticSystemSelectionButton->SetPressed(m_automaticSystemSelection);
		return;
	}

	bool reset_view = false;

	// fast move selection to current player system or hyperspace target
	const bool shifted = (Pi::KeyState(SDLK_LSHIFT) || Pi::KeyState(SDLK_RSHIFT));
	if (KeyBindings::mapWarpToCurrent.Matches(keysym)) {
		GotoSystem(m_current);
		reset_view = shifted;
	} else if (KeyBindings::mapWarpToSelected.Matches(keysym)) {
		GotoSystem(m_selected);
		reset_view = shifted;
	} else if (KeyBindings::mapWarpToHyperspaceTarget.Matches(keysym)) {
		GotoSystem(m_hyperspaceTarget);
		reset_view = shifted;
	}

	// reset rotation and zoom
	if (reset_view || KeyBindings::mapViewReset.Matches(keysym)) {
		while (m_rotZ < -180.0f) m_rotZ += 360.0f;
		while (m_rotZ > 180.0f)  m_rotZ -= 360.0f;
		m_rotXMovingTo = m_rotXDefault;
		m_rotZMovingTo = m_rotZDefault;
		m_zoomMovingTo = m_zoomDefault;
	}
}

void SectorView::Update()
{
	PROFILE_SCOPED()
	SystemPath last_current = m_current;
	bool last_inSystem = m_inSystem;

	if (Pi::game->IsNormalSpace()) {
		m_inSystem = true;
		m_current = Pi::game->GetSpace()->GetStarSystem()->GetSystemPath();
	}
	else {
		m_inSystem = false;
		m_current = Pi::player->GetHyperspaceDest();
	}

	if (last_inSystem != m_inSystem || last_current != m_current) {
		UpdateSystemLabels(m_currentSystemLabels, m_current);
		UpdateSystemLabels(m_targetSystemLabels, m_hyperspaceTarget);
		UpdateSystemLabels(m_selectedSystemLabels, m_selected);
		UpdateDistanceLabelAndLine(m_secondDistance, m_selected, m_hyperspaceTarget);
	}

	const float frameTime = Pi::GetFrameTime();

	matrix4x4f rot = matrix4x4f::Identity();
	rot.RotateX(DEG2RAD(-m_rotX));
	rot.RotateZ(DEG2RAD(-m_rotZ));

	// don't check raw keypresses if the search box is active
	if (!m_searchBox->IsFocused()) {
		const float moveSpeed = Pi::GetMoveSpeedShiftModifier();
		float move = moveSpeed*frameTime;
		vector3f shift(0.0f);
		if (KeyBindings::mapViewShiftLeft.IsActive()) shift.x -= move;
		if (KeyBindings::mapViewShiftRight.IsActive()) shift.x += move;
		if (KeyBindings::mapViewShiftUp.IsActive()) shift.y += move;
		if (KeyBindings::mapViewShiftDown.IsActive()) shift.y -= move;
		if (KeyBindings::mapViewShiftForward.IsActive()) shift.z -= move;
		if (KeyBindings::mapViewShiftBackward.IsActive()) shift.z += move;
		m_posMovingTo += shift * rot;

		if (KeyBindings::viewZoomIn.IsActive() || m_zoomInButton->IsPressed())
			m_zoomMovingTo -= move;
		if (KeyBindings::viewZoomOut.IsActive() || m_zoomOutButton->IsPressed())
			m_zoomMovingTo += move;
		m_zoomMovingTo = Clamp(m_zoomMovingTo, 0.1f, FAR_LIMIT);

		if (KeyBindings::mapViewRotateLeft.IsActive()) m_rotZMovingTo -= 0.5f * moveSpeed;
		if (KeyBindings::mapViewRotateRight.IsActive()) m_rotZMovingTo += 0.5f * moveSpeed;
		if (KeyBindings::mapViewRotateUp.IsActive()) m_rotXMovingTo -= 0.5f * moveSpeed;
		if (KeyBindings::mapViewRotateDown.IsActive()) m_rotXMovingTo += 0.5f * moveSpeed;
	}

	if (Pi::MouseButtonState(SDL_BUTTON_RIGHT)) {
		int motion[2];
		Pi::GetMouseMotion(motion);

		m_rotXMovingTo += 0.2f*float(motion[1]);
		m_rotZMovingTo += 0.2f*float(motion[0]);
	}

	m_rotXMovingTo = Clamp(m_rotXMovingTo, -170.0f, -10.0f);

	{
		vector3f diffPos = m_posMovingTo - m_pos;
		vector3f travelPos = diffPos * 10.0f*frameTime;
		if (travelPos.Length() > diffPos.Length()) m_pos = m_posMovingTo;
		else m_pos = m_pos + travelPos;

		float diffX = m_rotXMovingTo - m_rotX;
		float travelX = diffX * 10.0f*frameTime;
		if (fabs(travelX) > fabs(diffX)) m_rotX = m_rotXMovingTo;
		else m_rotX = m_rotX + travelX;

		float diffZ = m_rotZMovingTo - m_rotZ;
		float travelZ = diffZ * 10.0f*frameTime;
		if (fabs(travelZ) > fabs(diffZ)) m_rotZ = m_rotZMovingTo;
		else m_rotZ = m_rotZ + travelZ;

		float diffZoom = m_zoomMovingTo - m_zoom;
		float travelZoom = diffZoom * ZOOM_SPEED*frameTime;
		if (fabs(travelZoom) > fabs(diffZoom)) m_zoom = m_zoomMovingTo;
		else m_zoom = m_zoom + travelZoom;
		m_zoomClamped = Clamp(m_zoom, 1.f, FAR_LIMIT);
	}

	if (m_automaticSystemSelection) {
		SystemPath new_selected = SystemPath(int(floor(m_pos.x)), int(floor(m_pos.y)), int(floor(m_pos.z)), 0);

		RefCountedPtr<Sector> ps = GetCached(new_selected);
		if (ps->m_systems.size()) {
			float px = FFRAC(m_pos.x)*Sector::SIZE;
			float py = FFRAC(m_pos.y)*Sector::SIZE;
			float pz = FFRAC(m_pos.z)*Sector::SIZE;

			float min_dist = FLT_MAX;
			for (unsigned int i=0; i<ps->m_systems.size(); i++) {
				Sector::System *ss = &ps->m_systems[i];
				float dx = px - ss->p.x;
				float dy = py - ss->p.y;
				float dz = pz - ss->p.z;
				float dist = sqrtf(dx*dx + dy*dy + dz*dz);
				if (dist < min_dist) {
					min_dist = dist;
					new_selected.systemIndex = i;
				}
			}

			if (!m_selected.IsSameSystem(new_selected)) {
				RefCountedPtr<StarSystem> system = StarSystem::cache->GetCached(new_selected);
				SetSelected(system->GetStars()[0]->GetPath());
			}
		}
	}

	ShrinkCache();

	UIView::Update();
}

void SectorView::ShowAll()
{
	View::ShowAll();
	if (m_detailBoxVisible != DETAILBOX_INFO)    m_infoBox->HideAll();
}

void SectorView::MouseWheel(bool up)
{
	if (this == Pi::GetView()) {
		if (!up)
			m_zoomMovingTo += ZOOM_SPEED * WHEEL_SENSITIVITY * Pi::GetMoveSpeedShiftModifier();
		else
			m_zoomMovingTo -= ZOOM_SPEED * WHEEL_SENSITIVITY * Pi::GetMoveSpeedShiftModifier();
	}
}

void SectorView::ShrinkCache()
{
	PROFILE_SCOPED()
	// we're going to use these to determine if our sectors are within the range that we'll ever render
	const int drawRadius = DRAW_RAD;

	const int xmin = int(floorf(m_pos.x))-drawRadius;
	const int xmax = int(floorf(m_pos.x))+drawRadius;
	const int ymin = int(floorf(m_pos.y))-drawRadius;
	const int ymax = int(floorf(m_pos.y))+drawRadius;
	const int zmin = int(floorf(m_pos.z))-drawRadius;
	const int zmax = int(floorf(m_pos.z))+drawRadius;

	// XXX don't clear the current/selected/target sectors

	if  (xmin != m_cacheXMin || xmax != m_cacheXMax
	  || ymin != m_cacheYMin || ymax != m_cacheYMax
	  || zmin != m_cacheZMin || zmax != m_cacheZMax) {
		auto iter = m_sectorCache->Begin();
		while (iter != m_sectorCache->End())	{
			RefCountedPtr<Sector> s = iter->second;
			//check_point_in_box
			if (!s->WithinBox( xmin, xmax, ymin, ymax, zmin, zmax )) {
				m_sectorCache->Erase( iter++ );
			} else {
				iter++;
			}
		}

		m_cacheXMin = xmin;
		m_cacheXMax = xmax;
		m_cacheYMin = ymin;
		m_cacheYMax = ymax;
		m_cacheZMin = zmin;
		m_cacheZMax = zmax;
	}
}
