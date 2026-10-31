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

// FILE: TiberiumCrystalUpdate.h //////////////////////////////////////////////////////////////////
// Tiberium Project @feature Kane's Wrath's Tiberium crystal: a supply source (SupplyWarehouseDockUpdate,
// its boxes; BoxWorth for the richer kinds) that grows back what harvesters took. RegrowDelay after the
// last box taken, it gains RegrowRate boxes a second up to its StartingBoxes. An emptied crystal grows
// back from nothing (W3DSupplyDraw's ScaleBySupply draws it as it grows). While it holds any, it hurts
// what stands in it: DamagePerSecond to the DamageKindOf (infantry) within DamageRadius, of DamageType
// (radiation by default: poison would go on hurting after, by the infantry's PoisonedBehavior) and
// DeathType (poisoned).
//
//   Behavior = TiberiumCrystalUpdate ModuleTag_Growth
//     RegrowDelay = 15000     ; msec after the last box taken
//     RegrowRate = 0.5        ; boxes a second
//     DamageRadius = 16
//     DamagePerSecond = 4
//     DamageKindOf = INFANTRY
//     DamageType = RADIATION
//     DeathType = POISONED
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/KindOf.h"
#include "GameLogic/Damage.h"
#include "GameLogic/Module/UpdateModule.h"

//-------------------------------------------------------------------------------------------------
class TiberiumCrystalUpdateModuleData : public UpdateModuleData
{
public:
	UnsignedInt m_regrowDelay;
	Real m_regrowRate;
	Real m_damageRadius;
	Real m_damagePerSecond;
	KindOfMaskType m_damageKindOf;
	DamageType m_damageType;
	DeathType m_deathType;

	TiberiumCrystalUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class TiberiumCrystalUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( TiberiumCrystalUpdate, "TiberiumCrystalUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( TiberiumCrystalUpdate, TiberiumCrystalUpdateModuleData )

public:

	TiberiumCrystalUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:

	void hurtWhatStandsInIt();

	Int m_lastStored;					///< the boxes at the last look (fewer now: a harvester took some)
	UnsignedInt m_growFrame;	///< when it may grow again
	Real m_growth;						///< part of a box grown

};
