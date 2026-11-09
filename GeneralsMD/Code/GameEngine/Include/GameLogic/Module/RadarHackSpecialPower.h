/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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


// FILE: RadarHackSpecialPower.h //////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's RadarHackSpecialPower (Kane's Wrath's Venom with its Signature Generator): used,
// the unit makes NumFalseReturns false returns on the radar within Radius of it for HackDuration: blips of its side's
// colour that move about it, wherever a player's radar would show a unit there (in sight).
//
//   Behavior = RadarHackSpecialPower ModuleTag_SignatureGenerator
//     SpecialPowerTemplate = KWSP_NODSignatureGenerator
//     NumFalseReturns      = 5
//     Radius               = 102
//     HackDuration         = 20000
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class Waypoint;

#include "GameLogic/Module/SpecialPowerModule.h"

//-------------------------------------------------------------------------------------------------
class RadarHackSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	UnsignedInt m_numFalseReturns;
	Real m_radius;
	UnsignedInt m_hackDuration;		///< frames

	RadarHackSpecialPowerModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class RadarHackSpecialPower : public SpecialPowerModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( RadarHackSpecialPower, "RadarHackSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( RadarHackSpecialPower, RadarHackSpecialPowerModuleData )

public:

	RadarHackSpecialPower( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool object

	virtual void doSpecialPower( UnsignedInt commandOptions ) override;
	virtual void doSpecialPowerAtObject( Object *obj, UnsignedInt commandOptions ) override;
	virtual void doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions ) override;

private:

	void startHack();
	void showOnRadar() const;		///< the drawable carries the false returns for the radar

	UnsignedInt m_hackEndFrame;		///< 0: none
};
