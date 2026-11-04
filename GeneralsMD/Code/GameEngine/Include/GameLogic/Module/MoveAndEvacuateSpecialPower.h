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

// FILE: MoveAndEvacuateSpecialPower.h ///////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's MoveToPositionAndEvacuateSpecialPower: a transport flies or drives
// to the spot it is given and lets everyone out there (the Ox's, the Carryall's and the APCs' button). Use
// with a SpecialPower whose button needs a target position, e.g.
//
//   Behavior = MoveAndEvacuateSpecialPower ModuleTag_Evacuate
//     SpecialPowerTemplate = C4SP_SpecialPowerMoveToPositionAndEvacuate
//   End
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/SpecialPowerModule.h"

//-------------------------------------------------------------------------------------------------
class MoveAndEvacuateSpecialPower : public SpecialPowerModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( MoveAndEvacuateSpecialPower, "MoveAndEvacuateSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( MoveAndEvacuateSpecialPower, SpecialPowerModuleData )

public:

	MoveAndEvacuateSpecialPower( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype defined by MemoryPoolObject

	virtual void doSpecialPowerAtLocation( const Coord3D *loc, Real angle, UnsignedInt commandOptions ) override;
};
