// Copyright © 2008-2014 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "libs.h"
#include "Pi.h"
#include "BaseSphere.h"
#include "GeoSphere.h"
#include "GasGiant.h"
#include "graphics/Material.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"

RefCountedPtr<Graphics::VertexBuffer> BaseSphere::m_vLongBuffer;
RefCountedPtr<Graphics::VertexBuffer> BaseSphere::m_vLatBuffer;

BaseSphere::BaseSphere(const SystemBody *body) : m_sbody(body), m_terrain(Terrain::InstanceTerrain(body)) {}

BaseSphere::~BaseSphere() {}

//static 
void BaseSphere::Init()
{
	GeoSphere::Init();
}

//static 
void BaseSphere::Uninit()
{
	GeoSphere::Uninit();
}

//static 
void BaseSphere::UpdateAllBaseSphereDerivatives()
{
	GeoSphere::UpdateAllGeoSpheres();
	GasGiant::UpdateAllGasGiants();
}

//static 
void BaseSphere::OnChangeDetailLevel()
{
	GeoSphere::OnChangeDetailLevel();
}

//static 
void BaseSphere::DrawAtmosphereSurface(Graphics::Renderer *renderer,
	const matrix4x4d &modelView, const vector3d &campos, float rad,
	Graphics::RenderState *rs, Graphics::Material *mat)
{
	const int LAT_SEGS = 20;
	const int LONG_SEGS = 20;
	vector3d yaxis = campos.Normalized();
	vector3d zaxis = vector3d(1.0,0.0,0.0).Cross(yaxis).Normalized();
	vector3d xaxis = yaxis.Cross(zaxis);
	const matrix4x4d invrot = matrix4x4d::MakeRotMatrix(xaxis, yaxis, zaxis).Inverse();

	renderer->SetTransform(modelView * matrix4x4d::ScaleMatrix(rad, rad, rad) * invrot);

	// what is this? Well, angle to the horizon is:
	// acos(planetRadius/viewerDistFromSphereCentre)
	// and angle from this tangent on to atmosphere is:
	// acos(planetRadius/atmosphereRadius) ie acos(1.0/1.01244blah)
	double endAng = acos(1.0/campos.Length())+acos(1.0/rad);
	double latDiff = endAng / double(LAT_SEGS);

	double rot = 0.0;
	float sinCosTable[LONG_SEGS+1][2];
	for (int i=0; i<=LONG_SEGS; i++, rot += 2.0*M_PI/double(LONG_SEGS)) {
		sinCosTable[i][0] = float(sin(rot));
		sinCosTable[i][1] = float(cos(rot));
	}

	/* Tri-fan above viewer */
	Graphics::VertexArray va(Graphics::ATTRIB_POSITION);
	va.Add(vector3f(0.f, 1.f, 0.f));
	for (int i=0; i<=LONG_SEGS; i++) {
		va.Add(vector3f(
			sin(latDiff)*sinCosTable[i][0],
			cos(latDiff),
			-sin(latDiff)*sinCosTable[i][1]));
	}

	if( !m_vLongBuffer.Valid() && va.GetNumVerts()>0 )
	{
		//create buffer and upload data
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.numVertices = va.GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_DYNAMIC;	// we could be updating this per-frame
		mat->SetupVertexBufferDesc( vbd );
		m_vLongBuffer.Reset( renderer->CreateVertexBuffer(vbd) );
	}
	m_vLongBuffer->Populate( va );

	renderer->DrawBuffer(m_vLongBuffer.Get(), rs, mat, Graphics::TRIANGLE_FAN);

	/* and wound latitudinal strips */
	double lat = latDiff;
	for (int j=1; j<LAT_SEGS; j++, lat += latDiff) {
		Graphics::VertexArray v(Graphics::ATTRIB_POSITION);
		float cosLat = cos(lat);
		float sinLat = sin(lat);
		float cosLat2 = cos(lat+latDiff);
		float sinLat2 = sin(lat+latDiff);
		for (int i=0; i<=LONG_SEGS; i++) {
			v.Add(vector3f(sinLat*sinCosTable[i][0], cosLat, -sinLat*sinCosTable[i][1]));
			v.Add(vector3f(sinLat2*sinCosTable[i][0], cosLat2, -sinLat2*sinCosTable[i][1]));
		}
		if( !m_vLatBuffer.Valid() && va.GetNumVerts()>0 )
		{
			//create buffer and upload data
			Graphics::VertexBufferDesc vbd;
			vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
			vbd.attrib[0].format   = Graphics::ATTRIB_FORMAT_FLOAT3;
			vbd.numVertices = va.GetNumVerts();
			vbd.usage = Graphics::BUFFER_USAGE_DYNAMIC;	// we could be updating this per-frame
			mat->SetupVertexBufferDesc( vbd );
			m_vLatBuffer.Reset( renderer->CreateVertexBuffer(vbd) );
		}
		m_vLatBuffer->Populate( va );
		renderer->DrawBuffer(m_vLatBuffer.Get(), rs, mat, Graphics::TRIANGLE_STRIP);
	}
}
