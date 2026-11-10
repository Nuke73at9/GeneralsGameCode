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

// FILE: ClusterBombUpdate.h //////////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE's ClusterBombUpdate (Kane's Wrath's and C&C4's Shockwave Artillery, the cluster bomb,
// the Steel Talons' Railgun Accelerator): from the moment the object is made, NumBomblets shots of Weapon, each at its own
// random spot within Radius of the object (0: on the object), each after its own random delay between MinDelay and
// MaxDelay, BombletFX played where each lands. InitialWeapon is fired at the object's own spot at once.
//
//   Behavior = ClusterBombUpdate ModuleTag_Bomblets
//     Weapon       = KWW_GDISpaceCommandShockwaveWeapon
//     Radius       = 204
//     NumBomblets  = 12
//     MinDelay     = 0
//     MaxDelay     = 5000
//     BombletFX    = KWFX_GDIShockwaveArtilery
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>

#include "GameLogic/Module/UpdateModule.h"

class FXList;
class WeaponTemplate;

//-------------------------------------------------------------------------------------------------
class ClusterBombUpdateModuleData : public UpdateModuleData
{
public:
	const WeaponTemplate *m_weapon;					///< each bomblet
	const WeaponTemplate *m_initialWeapon;	///< at once, on the object (none: nothing)
	Real m_radius;
	Int m_numBomblets;
	UnsignedInt m_minDelay;									///< frames from the object's making
	UnsignedInt m_maxDelay;
	const FXList *m_bombletFX;

	ClusterBombUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse& p );
};

//-------------------------------------------------------------------------------------------------
class ClusterBombUpdate : public UpdateModule
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( ClusterBombUpdate, "ClusterBombUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( ClusterBombUpdate, ClusterBombUpdateModuleData )

public:

	ClusterBombUpdate( Thing *thing, const ModuleData* moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:

	void fireBomblet();

	Bool m_started;
	std::vector<UnsignedInt> m_frames;	///< the bomblets still to come: when each falls, soonest first
};
