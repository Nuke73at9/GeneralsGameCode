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

// FILE: CreateAndEnterObjectSpecialPower.h //////////////////////////////////
// Tiberium Project @feature SAGE's CreateAndEnterObjectSpecialPower: the unit calls up an object and gets
// into it. Kane's Wrath's call for transport makes an Ox or a Carryall above the unit, which lands and
// picks it up (ChinookAIUpdate lands for whoever wants in); its infantry's dig in (a location power) makes a
// foxhole at the spot, which they walk into. The object is the OCL's first, its owner's. In Zero Hour a
// squad is its soldiers, each with the power: those told together (the same frame, player and power) share
// what the first made while it has room, as Kane's Wrath's squad calls one transport.
//
//   Behavior = CreateAndEnterObjectSpecialPower ModuleTag_CallForTransport
//     SpecialPowerTemplate = C4SP_SpecialPowerCallForTransportGDI
//     OCL = KWOCL_OCL_GDICallForTransport
//     TriggerFX = KWFX_FX_GDICallForTransportFlare    ; at the unit, as it calls
//   End
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

class FXList;
class ObjectCreationList;

//-------------------------------------------------------------------------------------------------
class CreateAndEnterObjectSpecialPowerModuleData : public SpecialPowerModuleData
{
public:
	const ObjectCreationList *m_ocl;
	const FXList *m_triggerFX;

	CreateAndEnterObjectSpecialPowerModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class CreateAndEnterObjectSpecialPower : public SpecialPowerModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( CreateAndEnterObjectSpecialPower, "CreateAndEnterObjectSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( CreateAndEnterObjectSpecialPower, CreateAndEnterObjectSpecialPowerModuleData )

public:

	CreateAndEnterObjectSpecialPower( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

	virtual void doSpecialPower( UnsignedInt commandOptions ) override;
	virtual void doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions ) override;

	/// a new game starts (GameLogic): forget the last squad's object
	static void resetLastMade();
	/// after the partition manager's update (GameLogic): the units sent into what was made this frame
	static void sendPendingRiders();

private:

	void createAndEnter( const Coord3D *at, Bool atUnit, UnsignedInt commandOptions );
};
