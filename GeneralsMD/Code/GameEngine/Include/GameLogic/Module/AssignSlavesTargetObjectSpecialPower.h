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


// FILE: AssignSlavesTargetObjectSpecialPower.h ///////////////////////////////////////////////////
// Tiberium Project @feature SAGE's AssignSlavesTargetObjectSpecialPower (Kane's Wrath's Steel Talons Repair APC): used on an
// object, the unit's drones (its SpawnBehavior's) repair that object next, until it is healed (SlavedUpdate's
// RepairOthersRange).
//
//   Behavior = AssignSlavesTargetObjectSpecialPower ModuleTag_RepairVehicle
//     SpecialPowerTemplate = KWSP_SpecialPower_TargetedRepairVehicle
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class Waypoint;

#include "GameLogic/Module/SpecialPowerModule.h"

//-------------------------------------------------------------------------------------------------
class AssignSlavesTargetObjectSpecialPowerModuleData : public SpecialPowerModuleData
{
};

//-------------------------------------------------------------------------------------------------
class AssignSlavesTargetObjectSpecialPower : public SpecialPowerModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( AssignSlavesTargetObjectSpecialPower, "AssignSlavesTargetObjectSpecialPower" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( AssignSlavesTargetObjectSpecialPower, AssignSlavesTargetObjectSpecialPowerModuleData )

public:

	AssignSlavesTargetObjectSpecialPower( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool object

	virtual void doSpecialPowerAtObject( Object *obj, UnsignedInt commandOptions ) override;
};
