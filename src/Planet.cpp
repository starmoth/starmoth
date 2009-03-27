#include "libs.h"
#include "Planet.h"
#include "Frame.h"
#include "Pi.h"
#include "WorldView.h"
#include "Serializer.h"
#include "StarSystem.h"
#include "GeoSphere.h"

struct ColRangeObj_t {
	float baseCol[4]; float modCol[4]; float modAll;

	void GenCol(float col[4], MTRand &rng) const {
		float ma = 1 + (rng.Double(modAll*2)-modAll);
		for (int i=0; i<4; i++) col[i] = baseCol[i] + rng.Double(-modCol[i], modCol[i]);
		for (int i=0; i<3; i++) col[i] = CLAMP(ma*col[i], 0, 1);
	}
};

ColRangeObj_t barrenBodyCol = { { .3,.3,.3,1 },{0,0,0,0},.3 };
ColRangeObj_t barrenContCol = { { .2,.2,.2,1 },{0,0,0,0},.3 };
ColRangeObj_t barrenEjectaCraterCol = { { .5,.5,.5,1 },{0,0,0,0},.2 };
float darkblue[4] = { .05, .05, .2, 1 };
float blue[4] = { .2, .2, 1, 1 };
float green[4] = { .2, .8, .2, 1 };
float white[4] = { 1, 1, 1, 1 };

Planet::Planet(SBody *sbody): Body()
{
	pos = vector3d(0,0,0);
	this->sbody = sbody;
	this->m_geosphere = 0;
	Init();
}

void Planet::Init()
{
	m_mass = sbody->GetMass();
	if ((!m_geosphere) &&
	    (sbody->type >= SBody::TYPE_PLANET_DWARF)) {
		float col[4];
		MTRand rand;	
		rand.seed(sbody->seed);
		m_geosphere = new GeoSphere();
		m_geosphere->AddCraters(rand, 20, M_PI*0.005, M_PI*0.05);
		switch (sbody->type){
		case SBody::TYPE_PLANET_DWARF:
		case SBody::TYPE_PLANET_SMALL:
			barrenBodyCol.GenCol(col, rand);
			m_geosphere->SetColor(col);
			break;
		case SBody::TYPE_PLANET_WATER:
		case SBody::TYPE_PLANET_WATER_THICK_ATMOS:
			m_geosphere->SetColor(darkblue);
			break;
		case SBody::TYPE_PLANET_INDIGENOUS_LIFE:
			m_geosphere->SetColor(green);
			break;
		default:
			barrenBodyCol.GenCol(col, rand);
			m_geosphere->SetColor(col);
		}
	}
	
	crudDList = 0;
}
	
void Planet::Save()
{
	using namespace Serializer::Write;
	Body::Save();
	wr_vector3d(pos);
	wr_int(Serializer::LookupSystemBody(sbody));
}

void Planet::Load()
{
	using namespace Serializer::Read;
	Body::Load();
	pos = rd_vector3d();
	sbody = Serializer::LookupSystemBody(rd_int());
	Init();
}

Planet::~Planet()
{
	if (m_geosphere) delete m_geosphere;
}

double Planet::GetRadius() const
{
	return sbody->GetRadius();
}

vector3d Planet::GetPosition() const
{
	return pos;
}

void Planet::SetPosition(vector3d p)
{
	pos = p;
}

void Planet::SetRadius(double radius)
{
	assert(0);
}

double Planet::GetTerrainHeight(vector3d &pos)
{
	double radius = GetRadius();
	if (m_geosphere) {
		return radius * (1.0 + m_geosphere->GetHeight(pos));
	} else {
		return radius;
	}
}

static void subdivide(vector3d &v1, vector3d &v2, vector3d &v3, vector3d &v4, int depth)
{
	if (depth) {
		depth--;
		vector3d v5 = (v1+v2).Normalized();
		vector3d v6 = (v2+v3).Normalized();
		vector3d v7 = (v3+v4).Normalized();
		vector3d v8 = (v4+v1).Normalized();
		vector3d v9 = (v1+v2+v3+v4).Normalized();

		subdivide(v1,v5,v9,v8,depth);
		subdivide(v5,v2,v6,v9,depth);
		subdivide(v9,v6,v3,v7,depth);
		subdivide(v8,v9,v7,v4,depth);
	} else {
		glBegin(GL_TRIANGLE_STRIP);
		glNormal3dv(&v1.x);
		glVertex3dv(&v1.x);
		glNormal3dv(&v2.x);
		glVertex3dv(&v2.x);
		glNormal3dv(&v4.x);
		glVertex3dv(&v4.x);
		glNormal3dv(&v3.x);
		glVertex3dv(&v3.x);
		glEnd();
	}
}

static void DrawShittyRoundCube(double radius)
{
	vector3d p1(1,1,1);
	vector3d p2(-1,1,1);
	vector3d p3(-1,-1,1);
	vector3d p4(1,-1,1);

	vector3d p5(1,1,-1);
	vector3d p6(-1,1,-1);
	vector3d p7(-1,-1,-1);
	vector3d p8(1,-1,-1);

	p1 = p1.Normalized();
	p2 = p2.Normalized();
	p3 = p3.Normalized();
	p4 = p4.Normalized();
	p5 = p5.Normalized();
	p6 = p6.Normalized();
	p7 = p7.Normalized();
	p8 = p8.Normalized();

//	p1 *= radius;
//	p2 *= radius;
//	p3 *= radius;
//	p4 *= radius;
//	p5 *= radius;
//	p6 *= radius;
//	p7 *= radius;
//	p8 *= radius;

//	glDisable(GL_CULL_FACE);
	glEnable(GL_NORMALIZE);
	subdivide(p1, p2, p3, p4, 4);
	subdivide(p4, p3, p7, p8, 4);
	subdivide(p1, p4, p8, p5, 4);
	subdivide(p2, p1, p5, p6, 4);
	subdivide(p3, p2, p6, p7, 4);
	subdivide(p8, p7, p6, p5, 4);
	
	glDisable(GL_NORMALIZE);
}

// both arguments in radians
void DrawHoop(float latitude, float width, float wobble, MTRand &rng)
{
	glPushAttrib(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glEnable(GL_BLEND);
	
	glBegin(GL_TRIANGLE_STRIP);
	for (double longitude=0.0f; longitude < 2*M_PI; longitude += 0.02) {
		vector3d v;
		double l;
		l = latitude+0.5*width+rng.Double(wobble*width);
		v.x = sin(longitude)*cos(l);
		v.y = sin(l);
		v.z = cos(longitude)*cos(l);
		v = v.Normalized();
		glNormal3dv(&v.x);
		glVertex3dv(&v.x);
		
		l = latitude-0.5*width-rng.Double(wobble*width);
		v.x = sin(longitude)*cos(l);
		v.y = sin(l);
		v.z = cos(longitude)*cos(l);
		glNormal3dv(&v.x);
		glVertex3dv(&v.x);
	}
	double l = latitude+0.5*width;
	vector3d v;
	v.x = 0; v.y = sin(l); v.z = cos(l);
	v = v.Normalized();
	glNormal3dv(&v.x);
	glVertex3dv(&v.x);
	
	l = latitude-0.5*width;
	v.x = 0; v.y = sin(l); v.z = cos(l);
	glNormal3dv(&v.x);
	glVertex3dv(&v.x);

	glEnd();

	glDisable(GL_BLEND);
	glDisable(GL_NORMALIZE);
	glPopAttrib();
}

static void PutPolarPoint(float latitude, float longitude)
{
	vector3d v;
	v.x = sin(longitude)*cos(latitude);
	v.y = sin(latitude);
	v.z = cos(longitude)*cos(latitude);
	v = v.Normalized();
	glNormal3dv(&v.x);
	glVertex3dv(&v.x);
}

void DrawBlob(float latitude, float longitude, float a, float b)
{
	glPushAttrib(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glEnable(GL_BLEND);

	glBegin(GL_TRIANGLE_FAN);
	PutPolarPoint(latitude, longitude);
	for (double theta=2*M_PI; theta>0; theta-=0.1) {
		double _lat = latitude + a * cos(theta);
		double _long = longitude + b * sin(theta);
		PutPolarPoint(_lat, _long);
	}
	{
		double _lat = latitude + a;
		double _long = longitude;
		PutPolarPoint(_lat, _long);
	}
	glEnd();

	glDisable(GL_BLEND);
	glDisable(GL_NORMALIZE);
	glPopAttrib();
}

static void DrawRing(double inner, double outer, const float color[4])
{
	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
		GL_ENABLE_BIT | GL_LIGHTING_BIT | GL_POLYGON_BIT);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glDisable(GL_CULL_FACE);

	glColor4fv(color);
	
	glBegin(GL_TRIANGLE_STRIP);
	glNormal3f(0,1,0);
	for (float ang=0; ang<2*M_PI; ang+=0.1) {
		glVertex3f(inner*sin(ang), 0, inner*cos(ang));
		glVertex3f(outer*sin(ang), 0, outer*cos(ang));
	}
	glVertex3f(0, 0, inner);
	glVertex3f(0, 0, outer);
	glEnd();

	//gluDisk(Pi::gluQuadric, inner, outer, 40, 20);
	glEnable(GL_CULL_FACE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
	glDisable(GL_BLEND);
	glDisable(GL_NORMALIZE);
	glPopAttrib();
}

static void SphereTriSubdivide(vector3d &v1, vector3d &v2, vector3d &v3, int depth)
{
	if (--depth > 0) {
		vector3d v4 = (v1+v2).Normalized();
		vector3d v5 = (v2+v3).Normalized();
		vector3d v6 = (v1+v3).Normalized();
		SphereTriSubdivide(v1,v4,v6,depth);
		SphereTriSubdivide(v4,v2,v5,depth);
		SphereTriSubdivide(v6,v4,v5,depth);
		SphereTriSubdivide(v6,v5,v3,depth);
	} else {
		glNormal3dv(&v1.x);
		glVertex3dv(&v1.x);
		glNormal3dv(&v2.x);
		glVertex3dv(&v2.x);
		glNormal3dv(&v3.x);
		glVertex3dv(&v3.x);
	}
}

// yPos should be 1.0 for north pole, -1.0 for south pole
// size in radians
static void DrawPole(double yPos, double size)
{
	glPushAttrib(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);
	glEnable(GL_BLEND);

	const bool southPole = yPos < 0;
	size = size*4/M_PI;
	
	vector3d center(0, yPos, 0);
	glBegin(GL_TRIANGLES);
	for (float ang=2*M_PI; ang>0; ang-=0.1) {
		vector3d v1(size*sin(ang), yPos, size*cos(ang));
		vector3d v2(size*sin(ang+0.1), yPos, size*cos(ang+0.1));
		v1 = v1.Normalized();
		v2 = v2.Normalized();
		if (southPole)
			SphereTriSubdivide(center, v2, v1, 4);
		else
			SphereTriSubdivide(center, v1, v2, 4);
	}
	glEnd();


	glDisable(GL_BLEND);
	glDisable(GL_NORMALIZE);
	glPopAttrib();
}

struct GasGiantDef_t {
	int hoopMin, hoopMax; float hoopWobble;
	int blobMin, blobMax;
	float poleMin, poleMax; // size range in radians. zero for no poles.
	float ringProbability;
	ColRangeObj_t ringCol;
	ColRangeObj_t bodyCol;
	ColRangeObj_t hoopCol;
	ColRangeObj_t blobCol;
	ColRangeObj_t poleCol;
};

static GasGiantDef_t ggdefs[] = {
{
	/* jupiter */
	30, 40, 0.05,
	20, 30,
	0, 0,
	0.5,
	{ { .61,.48,.384,.1 }, {0,0,0,.9}, 0.3 },
	{ { .99,.76,.62,1 }, { 0,.1,.1,0 }, 0.3 },
	{ { .99,.76,.62,.5 }, { 0,.1,.1,0 }, 0.3 },
	{ { .99,.76,.62,1 }, { 0,.1,.1,0 }, 0.7 },
}, {
	/* saturnish */
	10, 15, 0.0,
	8, 20, // blob range
	0.2, 0.2, // pole size
	0.5,
	{ { .61,.48,.384,.1 }, {0,0,0,.9}, 0.3 },
	{ { .87, .68, .39, 1 }, { 0,0,0,0 }, 0.1 },
	{ { .87, .68, .39, 1 }, { 0,0,0,0 }, 0.1 },
	{ { .87, .68, .39, 1 }, { 0,0,0,0 }, 0.1 },
	{ { .77, .58, .29, 1 }, { 0,0,0,0 }, 0.1 },
}, {
	/* neptunish */
	3, 6, 0.0,
	2, 6,
	0, 0,
	0.5,
	{ { .61,.48,.384,.1 }, {0,0,0,.9}, 0.3 },
	{ { .31,.44,.73,1 }, {0,0,0,0}, .05}, // body col
	{ { .31,.44,.73,0.5 }, {0,0,0,0}, .1},// hoop col
	{ { .21,.34,.54,1 }, {0,0,0,0}, .05},// blob col
}, {
	/* uranus-like *wink* */
	0, 0, 0.0,
	0, 0,
	0, 0,
	0.5,
	{ { .61,.48,.384,.1 }, {0,0,0,.9}, 0.3 },
	{ { .70,.85,.86,1 }, {.1,.1,.1,0}, 0 },
	{ { .70,.85,.86,1 }, {.1,.1,.1,0}, 0 },
	{ { .70,.85,.86,1 }, {.1,.1,.1,0}, 0 },
	{ { .70,.85,.86,1 }, {.1,.1,.1,0}, 0 }
}, {
	/* brown dwarf-like */
	0, 0, 0.05,
	10, 20,
	0, 0,
	0.5,
	{ { .81,.48,.384,.1 }, {0,0,0,.9}, 0.3 },
	{ { .4,.1,0,1 }, {0,0,0,0}, 0.1 },
	{ { .4,.1,0,1 }, {0,0,0,0}, 0.1 },
	{ { .4,.1,0,1 }, {0,0,0,0}, 0.1 },
},
};

#define PLANET_AMBIENT	0.1

static void SetMaterialColor(const float col[4])
{
	float mambient[4];
	mambient[0] = col[0]*PLANET_AMBIENT;
	mambient[1] = col[1]*PLANET_AMBIENT;
	mambient[2] = col[2]*PLANET_AMBIENT;
	mambient[3] = col[3];
	glMaterialfv (GL_FRONT, GL_AMBIENT, mambient);
	glMaterialfv (GL_FRONT, GL_DIFFUSE, col);
}

void Planet::DrawGasGiant()
{
//	MTRand rng((int)Pi::GetGameTime());
	MTRand rng(sbody->seed+9);
	float col[4];
	
	// just use a random gas giant flavour for the moment
	GasGiantDef_t &ggdef = ggdefs[rng.Int32(0,3)];

	ggdef.bodyCol.GenCol(col, rng);
	SetMaterialColor(col);
	DrawShittyRoundCube(1.0f);
	
	int n = rng.Int32(ggdef.hoopMin, ggdef.hoopMax);

	while (n-- > 0) {
		ggdef.hoopCol.GenCol(col, rng);
		SetMaterialColor(col);
		DrawHoop(rng.Double(0.9*M_PI)-0.45*M_PI, rng.Double(0.25), ggdef.hoopWobble, rng);
	}

	n = rng.Int32(ggdef.blobMin, ggdef.blobMax);
	while (n-- > 0) {
		float a = rng.Double(0.01, 0.03);
		float b = a+rng.Double(0.2)+0.1;
		ggdef.blobCol.GenCol(col, rng);
		SetMaterialColor(col);
		DrawBlob(rng.Double(-0.3*M_PI, 0.3*M_PI), rng.Double(2*M_PI), a, b);
	}

	if (ggdef.poleMin != 0) {
		float size = rng.Double(ggdef.poleMin, ggdef.poleMax);
		ggdef.poleCol.GenCol(col, rng);
		SetMaterialColor(col);
		DrawPole(1.0, size);
		DrawPole(-1.0, size);
	}
	
	if (rng.Double(1.0) < ggdef.ringProbability) {
		float pos = rng.Double(1.2,1.7);
		float end = pos + rng.Double(0.1, 1.0);
		end = MIN(end, 2.5);
		while (pos < end) {
			float size = rng.Double(0.1);
			ggdef.ringCol.GenCol(col, rng);
			DrawRing(pos, pos+size, col);
			pos += size;
		}
	}
}

static void _DrawAtmosphere(double rad1, double rad2, vector3d &pos, const float col[4])
{
	glPushMatrix();
	// face the camera dammit
	vector3d zaxis = (-pos).Normalized();
	vector3d xaxis = (vector3d::Cross(zaxis, vector3d(0,1,0))).Normalized();
	vector3d yaxis = vector3d::Cross(zaxis,xaxis);
	matrix4x4d rot = matrix4x4d::MakeRotMatrix(xaxis, yaxis, zaxis).InverseOf();
	glMultMatrixd(&rot[0]);

	const double angStep = M_PI/32;
	// find angle player -> centre -> tangent point
	// tangent is from player to surface of sphere
	float tanAng = acos(rad1 / pos.Length());

	// then we can put the fucking atmosphere on the horizon
	vector3d r1(0.0, 0.0, rad1);
	vector3d r2(0.0, 0.0, rad2);
	rot = matrix4x4d::RotateYMatrix(tanAng);
	r1 = rot * r1;
	r2 = rot * r2;

	rot = matrix4x4d::RotateZMatrix(angStep);

	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glBegin(GL_TRIANGLE_STRIP);
	for (float ang=0; ang<2*M_PI; ang+=angStep) {
		glColor4fv(col);
		glVertex3dv(&r1.x);
		glColor4f(0,0,0,0);
		glVertex3dv(&r2.x);
		r1 = rot * r1;
		r2 = rot * r2;
	}
	
	glEnd();
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_LIGHTING);
	glPopMatrix();
}

void Planet::DrawAtmosphere(double rad, vector3d &pos)
{
	if (sbody->type == SBody::TYPE_PLANET_SMALL) {
		const float c[4] = { .2, .2, .3, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.05, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_CO2_THICK_ATMOS) {
		const float c[4] = { .8, .8, .8, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.1, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_CO2) {
		const float c[4] = { .5, .5, .5, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.05, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_METHANE_THICK_ATMOS) {
		const float c[4] = { .2, .6, .3, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.1, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_METHANE) {
		const float c[4] = { .2, .6, .3, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.05, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_HIGHLY_VOLCANIC) {
		const float c[4] = { .5, .2, .2, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.05, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_WATER_THICK_ATMOS) {
		const float c[4] = { .8, .8, .8, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.1, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_WATER) {
		const float c[4] = { .2, .2, .4, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.05, pos, c);
	}
	else if (sbody->type == SBody::TYPE_PLANET_INDIGENOUS_LIFE) {
		const float c[4] = { .2, .2, .5, .8 };
		_DrawAtmosphere(rad*0.99, rad*1.05, pos, c);
	}
}

void Planet::Render(const Frame *a_camFrame)
{
	glPushMatrix();

	double rad = sbody->GetRadius();
	matrix4x4d ftran;
	Frame::GetFrameTransform(GetFrame(), a_camFrame, ftran);
	vector3d fpos = ftran * GetPosition();

	double apparent_size = rad / fpos.Length();
	double len = fpos.Length();
	double origLen = len;

	do {
		rad *= 0.25;
		fpos = 0.25*fpos;
		len *= 0.25;
	} while ((len-rad)*0.25 > 4*WORLDVIEW_ZNEAR);

	glTranslatef(fpos.x, fpos.y, fpos.z);
	glColor3f(1,1,1);

	if (apparent_size < 0.001) {
		if (crudDList) {
			glDeleteLists(crudDList, 1);
			crudDList = 0;
		}
		/* XXX WRONG. need to pick light from appropriate turd. */
		GLfloat col[4];
		glGetLightfv(GL_LIGHT0, GL_DIFFUSE, col);
		// face the camera dammit
		vector3d zaxis = fpos.Normalized();
		vector3d xaxis = vector3d::Cross(vector3d(0,1,0), zaxis).Normalized();
		vector3d yaxis = vector3d::Cross(zaxis,xaxis);
		matrix4x4d rot = matrix4x4d::MakeRotMatrix(xaxis, yaxis, zaxis).InverseOf();
		glMultMatrixd(&rot[0]);

		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		
		glEnable(GL_BLEND);
		glColor4f(col[0], col[1], col[2], 1);
		glBegin(GL_TRIANGLE_FAN);
		glVertex3f(0,0,0);
		glColor4f(col[0],col[1],col[2],0);
		
		const float spikerad = 0.005*len +  1e1*(1.0*sbody->GetRadius()*len)/origLen;
		{
			/* bezier with (0,0,0) control points */
			vector3f p0(0,spikerad,0), p1(spikerad,0,0);
			float t=0.1; for (int i=1; i<10; i++, t+= 0.1f) {
				vector3f p = (1-t)*(1-t)*p0 + t*t*p1;
				glVertex3fv(&p[0]);
			}
		}
		{
			vector3f p0(spikerad,0,0), p1(0,-spikerad,0);
			float t=0.1; for (int i=1; i<10; i++, t+= 0.1f) {
				vector3f p = (1-t)*(1-t)*p0 + t*t*p1;
				glVertex3fv(&p[0]);
			}
		}
		{
			vector3f p0(0,-spikerad,0), p1(-spikerad,0,0);
			float t=0.1; for (int i=1; i<10; i++, t+= 0.1f) {
				vector3f p = (1-t)*(1-t)*p0 + t*t*p1;
				glVertex3fv(&p[0]);
			}
		}
		{
			vector3f p0(-spikerad,0,0), p1(0,spikerad,0);
			float t=0.1; for (int i=1; i<10; i++, t+= 0.1f) {
				vector3f p = (1-t)*(1-t)*p0 + t*t*p1;
				glVertex3fv(&p[0]);
			}
		}
		glEnd();
		glDisable(GL_BLEND);

		glEnable(GL_LIGHTING);
		glEnable(GL_DEPTH_TEST);
	} else {
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		float fracH = WORLDVIEW_ZNEAR / Pi::GetScrAspect();
		// very conservative zfar...
		glFrustum(-WORLDVIEW_ZNEAR, WORLDVIEW_ZNEAR, -fracH, fracH, WORLDVIEW_ZNEAR, MAX(rad, WORLDVIEW_ZFAR));
		glMatrixMode(GL_MODELVIEW);

		vector3d campos = -fpos;
		ftran.ClearToRotOnly();
		campos = ftran.InverseOf() * campos;
		glMultMatrixd(&ftran[0]);
		glEnable(GL_NORMALIZE);
		glPushMatrix();
		glScalef(rad,rad,rad);
		
		// this is a rather brittle test..........
		if (sbody->type < SBody::TYPE_PLANET_DWARF) {
			if (!crudDList) {
				crudDList = glGenLists(1);
				glNewList(crudDList, GL_COMPILE);
				DrawGasGiant();
				glEndList();
			}
			glCallList(crudDList);
		} else {
			const float poo[4] = { 1,1,1,1};
			SetMaterialColor(poo);
			campos = campos * (1.0/rad);
			m_geosphere->Render(campos);
		}
		glPopMatrix();
		glDisable(GL_NORMALIZE);
		

		fpos = ftran.InverseOf() * fpos;

		DrawAtmosphere(rad, fpos);
		glClear(GL_DEPTH_BUFFER_BIT);

		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
	}
	glPopMatrix();
}

void Planet::SetFrame(Frame *f)
{
	if (GetFrame()) {
		GetFrame()->SetPlanetGeom(0, 0);
	}
	Body::SetFrame(f);
	if (f) {
		GetFrame()->SetPlanetGeom(0, 0);
	}
}

