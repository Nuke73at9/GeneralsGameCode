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


// FILE: FireGroupCenterUpdate.h //////////////////////////////////////////////////////////////////
// Tiberium Project @feature SAGE 2's FireGroupCenterUpdate (C&C4's Nod fires): once RequiredCount others of its
// owner's, of the kinds named, burn within Range of it, the group is complete: this one goes to the group's centre and
// fires Weapon at itself (C&C4's: it dies FIRESTORM_DETONATED, and its CreateObjectDie makes a firestorm there); the
// others in the group die with it (KillContributing), and so does it if the weapon left it alive (KillOwner).
//
//   Behavior = FireGroupCenterUpdate ModuleTag_Firestorm
//     Weapon = C4W_GenericKillOwnerFirestormWeapon
//     Range = 32
//     RequiredCount = 3
//     KindOf = HAS_FIRE_GROUP_CENTER_UPDATE
//     SamePlayer = Yes
//   End
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameLogic/Module/UpdateModule.h"
#include "Common/KindOf.h"

class WeaponTemplate;

//-------------------------------------------------------------------------------------------------
class FireGroupCenterUpdateModuleData : public UpdateModuleData
{
public:
	const WeaponTemplate *m_weaponTemplate;	///< fired at itself when the group is complete
	Real m_range;														///< how near the others burn
	UnsignedInt m_requiredCount;						///< how many others make the group
	KindOfMaskType m_kindOf;								///< the others are of any of these kinds (none named: any)
	Bool m_samePlayer;											///< the others are its owner's (SAGE's SAME_PLAYER), else its allies' too
	Bool m_killContributing;								///< the others in the group die as it fires
	Bool m_killOwner;												///< it dies as it fires, if its weapon did not kill it
	UnsignedInt m_scanFrames;								///< how often it looks

	FireGroupCenterUpdateModuleData();
	static void buildFieldParse( MultiIniFieldParse &p );
};

//-------------------------------------------------------------------------------------------------
class FireGroupCenterUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE( FireGroupCenterUpdate, "FireGroupCenterUpdate" )
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA( FireGroupCenterUpdate, FireGroupCenterUpdateModuleData )

public:
	FireGroupCenterUpdate( Thing *thing, const ModuleData *moduleData );
	// virtual destructor prototype provided by memory pool declaration

	virtual UpdateSleepTime update() override;

private:
	Bool m_triggered;	///< it has fired (once only)
};
