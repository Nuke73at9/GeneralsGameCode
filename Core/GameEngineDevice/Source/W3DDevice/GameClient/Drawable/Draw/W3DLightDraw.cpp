/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// FILE: W3DLightDraw.cpp /////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's LightDraw (see W3DLightDraw.h)
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Common/INI.h"
#include "GameClient/ClientRandomValue.h"
#include "Common/Thing.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "W3DDevice/GameClient/Module/W3DLightDraw.h"
#include "WW3D2/ww3d.h"
#include "WWMath/matrix3d.h"
#include "WWMath/wwmath.h"
#ifdef RTS_RENDERER_D3D9
#include "WW3D2/sage2/sage2scene.h"
#endif

//-------------------------------------------------------------------------------------------------
W3DLightDrawLight::W3DLightDrawLight() : m_spot(FALSE), m_innerRadius(0.0f), m_outerRadius(0.0f),
	m_innerAngle(20.0f), m_outerAngle(45.0f), m_pulseMin(1.0f), m_pulseMax(1.0f), m_pulseRate(0.0f)
{
	m_color[0] = m_color[1] = m_color[2] = 1.0f;
	m_offset[0] = m_offset[1] = m_offset[2] = 0.0f;
	m_direction[0] = 0.0f;
	m_direction[1] = 0.0f;
	m_direction[2] = -1.0f;
}

//-------------------------------------------------------------------------------------------------
W3DLightDrawModuleData::W3DLightDrawModuleData()
{
}

W3DLightDrawModuleData::~W3DLightDrawModuleData()
{
}

//-------------------------------------------------------------------------------------------------
void W3DLightDrawModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "PointLight",	W3DLightDrawModuleData::parsePointLight,	nullptr, 0 },
		{ "SpotLight",	W3DLightDrawModuleData::parseSpotLight,		nullptr, 0 },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

void W3DLightDrawModuleData::parsePointLight(INI* ini, void* instance, void* /*store*/, const void* /*userData*/)
{
	parseLight(ini, (W3DLightDrawModuleData*)instance, FALSE);
}

void W3DLightDrawModuleData::parseSpotLight(INI* ini, void* instance, void* /*store*/, const void* /*userData*/)
{
	parseLight(ini, (W3DLightDrawModuleData*)instance, TRUE);
}

//-------------------------------------------------------------------------------------------------
/** One light: Key:a,b,c tokens in any order (see W3DLightDraw.h). */
//-------------------------------------------------------------------------------------------------
void W3DLightDrawModuleData::parseLight(INI* ini, W3DLightDrawModuleData* data, Bool spot)
{
	W3DLightDrawLight light;
	light.m_spot = spot;
	for (const char* token = ini->getNextTokenOrNull(); token != nullptr; token = ini->getNextTokenOrNull())
	{
		const char* colon = strchr(token, ':');
		if (colon == nullptr)
			throw INI_INVALID_DATA;
		char key[32];
		size_t keyLength = (size_t)(colon - token);
		if (keyLength >= sizeof(key))
			throw INI_INVALID_DATA;
		memcpy(key, token, keyLength);
		key[keyLength] = '\0';
		float v[3] = { 0.0f, 0.0f, 0.0f };
		int n = sscanf(colon + 1, "%f,%f,%f", &v[0], &v[1], &v[2]);
		if (n < 1)
			throw INI_INVALID_DATA;
		if (stricmp(key, "Color") == 0 && n == 3)
		{
			light.m_color[0] = v[0];
			light.m_color[1] = v[1];
			light.m_color[2] = v[2];
		}
		else if (stricmp(key, "Radius") == 0 && n == 2)
		{
			light.m_innerRadius = v[0];
			light.m_outerRadius = v[1];
		}
		else if (stricmp(key, "Offset") == 0 && n == 3)
		{
			light.m_offset[0] = v[0];
			light.m_offset[1] = v[1];
			light.m_offset[2] = v[2];
		}
		else if (stricmp(key, "Direction") == 0 && n == 3)
		{
			light.m_direction[0] = v[0];
			light.m_direction[1] = v[1];
			light.m_direction[2] = v[2];
		}
		else if (stricmp(key, "Angles") == 0 && n == 2)
		{
			light.m_innerAngle = v[0];
			light.m_outerAngle = v[1];
		}
		else if (stricmp(key, "Pulse") == 0 && n == 3)
		{
			light.m_pulseMin = v[0];
			light.m_pulseMax = v[1];
			light.m_pulseRate = v[2];
		}
		else
		{
			throw INI_INVALID_DATA;
		}
	}
	if (light.m_outerRadius <= 0.0f || light.m_outerRadius < light.m_innerRadius)
		throw INI_INVALID_DATA;
	data->m_lights.push_back(light);
}

//-------------------------------------------------------------------------------------------------
W3DLightDraw::W3DLightDraw( Thing *thing, const ModuleData* moduleData ) : DrawModule( thing, moduleData )
{
	m_obscured = FALSE;
	m_phase = GameClientRandomValueReal(0.0f, 1.0f);
}

//-------------------------------------------------------------------------------------------------
W3DLightDraw::~W3DLightDraw()
{
}

//-------------------------------------------------------------------------------------------------
/** Hands this frame's lights to the deferred light pass (the drawables update before the frame) */
//-------------------------------------------------------------------------------------------------
void W3DLightDraw::doDrawModule(const Matrix3D* transformMtx)
{
#ifdef RTS_RENDERER_D3D9
	const Drawable* draw = getDrawable();
	if (transformMtx == nullptr || draw == nullptr || m_obscured || draw->isDrawableEffectivelyHidden())
		return;
	const W3DLightDrawModuleData* data = getW3DLightDrawModuleData();
	const Real seconds = (Real)WW3D::Get_Sync_Time() * 0.001f;
	for (size_t i = 0; i < data->m_lights.size(); ++i)
	{
		const W3DLightDrawLight& light = data->m_lights[i];
		Real scale = 1.0f;
		if (light.m_pulseRate > 0.0f)
		{
			const Real cycle = seconds * light.m_pulseRate + m_phase;
			const Real wave = 0.5f + 0.5f * WWMath::Sin(cycle * 2.0f * WWMATH_PI);
			scale = light.m_pulseMin + (light.m_pulseMax - light.m_pulseMin) * wave;
		}
		Sage2SceneClass::PointLight point;
		Vector3 position;
		Matrix3D::Transform_Vector(*transformMtx, Vector3(light.m_offset[0], light.m_offset[1], light.m_offset[2]), &position);
		point.position[0] = position.X;
		point.position[1] = position.Y;
		point.position[2] = position.Z;
		point.color[0] = light.m_color[0] * scale;
		point.color[1] = light.m_color[1] * scale;
		point.color[2] = light.m_color[2] * scale;
		point.innerRadius = light.m_innerRadius;
		point.outerRadius = light.m_outerRadius;
		point.lightsObjects = true;		// nothing else lights the units around it with this light
		point.lightsParticles = true;
		if (light.m_spot)
		{
			Vector3 direction = transformMtx->Rotate_Vector(Vector3(light.m_direction[0], light.m_direction[1], light.m_direction[2]));
			const Real length = direction.Length();
			if (length <= 0.0f)
				continue;
			direction /= length;
			point.spot = true;
			point.direction[0] = direction.X;
			point.direction[1] = direction.Y;
			point.direction[2] = direction.Z;
			point.cosInner = WWMath::Cos(DEG_TO_RADF(light.m_innerAngle));
			point.cosOuter = WWMath::Cos(DEG_TO_RADF(light.m_outerAngle));
		}
		Sage2SceneClass::Add_Draw_Light(point);
	}
#else
	(void)transformMtx;
#endif
}

// ------------------------------------------------------------------------------------------------
void W3DLightDraw::crc( Xfer *xfer )
{
	DrawModule::crc( xfer );
}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void W3DLightDraw::xfer( Xfer *xfer )
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );
	DrawModule::xfer( xfer );
}

// ------------------------------------------------------------------------------------------------
void W3DLightDraw::loadPostProcess()
{
	DrawModule::loadPostProcess();
}
