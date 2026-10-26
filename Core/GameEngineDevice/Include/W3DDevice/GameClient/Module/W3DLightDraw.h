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

// FILE: W3DLightDraw.h ///////////////////////////////////////////////////////////////////////////
// Tiberium Project @feature C&C4's LightDraw: an object that gives off light where it is (a shell's
// glow in flight, a crawler's floodlights), as point and spot lights of the SAGE 2 deferred light
// pass. It draws nothing itself, and only on the Direct3D 9 renderer's C&C4 look; elsewhere the
// module reads its INI and does nothing. Client side only.
//
//   Draw = W3DLightDraw ModuleTag_Light
//     PointLight = Color:0.15,0.07,0.86 Radius:15,30 Offset:0,0,0
//     SpotLight  = Color:1,1,1 Radius:6,150 Angles:15,60 Offset:45,12,18 Direction:1,0,-0.5 Pulse:0.5,1.75,0.75
//   End
//
// Color: at the centre, as C&C4 gives it (linear, may pass 1); Radius: full strength to the first,
// nothing past the second (world units); Offset and Direction: in the object's own space (x forward,
// z up); Angles: a spot's inner and outer half angles in degrees; Pulse (optional): the colour scaled
// between a minimum and a maximum, so many cycles a second.
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/DrawModule.h"
#include <vector>

struct W3DLightDrawLight
{
	Bool m_spot;
	Real m_color[3];
	Real m_innerRadius;
	Real m_outerRadius;
	Real m_offset[3];
	Real m_direction[3];
	Real m_innerAngle;	///< degrees, from the axis
	Real m_outerAngle;
	Real m_pulseMin;
	Real m_pulseMax;
	Real m_pulseRate;		///< cycles a second; 0: steady
	W3DLightDrawLight();
};

class W3DLightDrawModuleData : public ModuleData
{
public:
	std::vector<W3DLightDrawLight> m_lights;

	W3DLightDrawModuleData();
	virtual ~W3DLightDrawModuleData() override;
	static void buildFieldParse(MultiIniFieldParse& p);

private:
	static void parsePointLight(INI* ini, void* instance, void* store, const void* userData);
	static void parseSpotLight(INI* ini, void* instance, void* store, const void* userData);
	static void parseLight(INI* ini, W3DLightDrawModuleData* data, Bool spot);
};

//-------------------------------------------------------------------------------------------------
class W3DLightDraw : public DrawModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( W3DLightDraw, "W3DLightDraw" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( W3DLightDraw, W3DLightDrawModuleData )

public:

	W3DLightDraw( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual void doDrawModule(const Matrix3D* transformMtx) override;
	virtual void setShadowsEnabled(Bool enable) override { }
	virtual void releaseShadows() override { }
	virtual void allocateShadows() override { }
	virtual void setFullyObscuredByShroud(Bool fullyObscured) override { m_obscured = fullyObscured; }
	virtual void reactToTransformChange(const Matrix3D* oldMtx, const Coord3D* oldPos, Real oldAngle) override { }
	virtual void reactToGeometryChange() override { }

protected:

	Bool m_obscured;		///< under the shroud: gives off no light
	Real m_phase;				///< where the pulse is, in cycles (each object its own start)

};
